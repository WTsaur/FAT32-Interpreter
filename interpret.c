#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "BPB.h"
#include "DIRENTRY.h"

void trimStringRight(char *str);
char *padRight(char *string, int padded_len, char *pad);

static int fatFD;
static unsigned int CurClus; /* aka cluster number for current working directory */
static BPB BootSec;
static unsigned int CurDataSec;

typedef struct
{
    int size;
    char **items;
} tokenlist;

void printInfo();
void ls(tokenlist *tokens);
void cd(tokenlist *tokens);
void rm(char *filename);
void trimStringRight(char *str);
int HiLoClusConvert(unsigned short HI, unsigned short LO);                /* converts DIRENTRY's FstClusHi and FstClusLo to a cluster number */
int getDataSecForClus(int N);                                             /* calculates the data sector for a given cluster, N */
int searchForDirClusNum(char *dirname);                                   /* searches cwd for dir and returns the cluster num for that dir */
int searchForDirClusNum_H(tokenlist *dirTokens, int curIdx, int cluster); /* helper func for searchForDirClusNum */
unsigned int clusterToFatAddress(unsigned int clusterNum);                /*  Takes cluster number and returns fat address*/
int create(char *filename, int isDirectory);
void createNewEntry(DIRENTRY *entry, int isDirectory, unsigned int address, char *name); /*Creates empty DIRENTRY object of type file or directory*/

tokenlist *new_tokenlist(void);
tokenlist *get_tokens(char *input, char *delims);
char *get_input(void);
void add_token(tokenlist *tokens, char *item);
void free_tokens(tokenlist *tokens);

int main(int argc, char *argv[])
{
    char *fatFile;

    if (argc != 2)
    {
        printf("error: usage: %s <FAT32 File>\n", argv[0]);
        exit(1);
    }

    fatFile = argv[1];
    fatFD = open(fatFile, O_RDWR);
    if (fatFD == -1)
    {
        printf("error opening FAT32 image file: %s\n", fatFile);
    }

    /* Retrieve BPB Info */
    unsigned char *buf;
    buf = (unsigned char *)malloc(512);
    if (read(fatFD, buf, 512) == -1)
    {
        printf("error reading boot sector");
    }
    memcpy(&BootSec, buf, sizeof(BPB));

    CurClus = BootSec.RootClus;

    //This stores the currrent directory. Intialize to root. (ReserveCount+(#Fat * FatSize)) * bytesPerSector
    CurDataSec = getDataSecForClus(CurClus);

    // printf("%i\n", CurDataSec);

    while (1)
    {
        printf("$ ");
        char *input = get_input();
        tokenlist *tokens = get_tokens(input, " ");
        if (tokens->size == 0)
            continue;

        char *command = tokens->items[0];
        if (strcmp(command, "info") == 0)
        {
            if (tokens->size == 1)
                printInfo();
            else
                printf("error: usage: info\n");
        }
        else if (strcmp(command, "size") == 0)
        {
            //size();
        }
        else if (strcmp(command, "ls") == 0)
        {
            if (tokens->size < 3)
                ls(tokens);
            else
                printf("error: usage: ls <DIR NAME>\n");
        }
        else if (strcmp(command, "cd") == 0)
        {
            if (tokens->size < 3)
                cd(tokens);
            else
                printf("error: usage: cd <DIR NAME>\n");
        }
        else if (strcmp(command, "creat") == 0)
        {
            if (tokens->size < 2)
                printf("error: usage: creat <FILE NAME>\n");
            else
                create(tokens->items[1], 0);
        }
        else if (strcmp(command, "mkdir") == 0)
        {
            if (tokens->size < 2)
                printf("error: usage: mkdir <DIR NAME>\n");
            else
                create(tokens->items[1], 1);
        }
        else if (strcmp(command, "mv") == 0)
        {
            //mv();
        }
        else if (strcmp(command, "open") == 0)
        {
            //open();
        }
        else if (strcmp(command, "close") == 0)
        {
            //close();
        }
        else if (strcmp(command, "lseek") == 0)
        {
            //lseek();
        }
        else if (strcmp(command, "read") == 0)
        {
            //read();
        }
        else if (strcmp(command, "write") == 0)
        {
            //write();
        }
        else if (strcmp(command, "rm") == 0)
        {
            if (tokens->size < 2)
                printf("error: usage: mkdir <DIR NAME>\n");
            else
                rm(tokens->items[1]);
        }
        else if (strcmp(command, "cp") == 0)
        {
            //cp();
        }
        else if (strcmp(command, "rmdir") == 0)
        {
            //rmdir();
        }
        else if (strcmp(command, "exit") == 0)
        {
            //exit
            free(buf);
            free_tokens(tokens);
            break;
        }
        else
        {
            printf("invalid command: %s\n", command);
        }
    }

    exit(0);
}

void printInfo()
{
    printf("Bytes Per Sector: %i\n", BootSec.BytesPerSec);
    printf("Sectors Per Cluser: %i\n", BootSec.SecPerClus);
    printf("Reserved Sector Count: %i\n", BootSec.RsvdSecCnt);
    printf("Number of FATs: %i\n", BootSec.NumFATs);
    printf("Total Sectors: %i\n", BootSec.TotSec32);
    printf("FAT Size: %i\n", BootSec.FATSz32);
    printf("Root Cluster: 0x%.2X\n", BootSec.RootClus);
}

int create(char *filename, int isDirectory)
{
    if (strlen(filename) > 8)
    {
        printf("name must not exceed 8 Characters\n");
        return -1;
    }
    unsigned int FREE_CLUSTER = 0x00000000;
    unsigned int FAT_END = 0x0FFFFFF8;

    DIRENTRY dirEntry;
    DIRENTRY testDir;
    //STEP 1 Find free fat entry
    int i = 0;

    // sectors_per_cluster
    unsigned int cluster_count = (BootSec.TotSec32 - (BootSec.RsvdSecCnt + (BootSec.NumFATs * BootSec.FATSz32)) * BootSec.BytesPerSec) / BootSec.SecPerClus;
    unsigned int fat_entry;
    unsigned int data_write_location;
    unsigned int fat_write_address;
    unsigned int fat_cluster;
    while (i < cluster_count)
    {

        fat_write_address = clusterToFatAddress(i);
        lseek(fatFD, fat_write_address, SEEK_SET);
        read(fatFD, &fat_entry, sizeof(fat_entry));
        if (fat_entry == FREE_CLUSTER)
        {
            fat_cluster = i;
            break;
        }
        i++;
    }

    //STEP 2 WRITE NEW FAT END
    lseek(fatFD, fat_write_address, SEEK_SET);
    write(fatFD, &FAT_END, 4); //write new file end

    //STEP 3 Create New Entry
    createNewEntry(&dirEntry, isDirectory, fat_cluster, filename);

    //STEP 4 WRITE FILE ENTRY at next open data address
    int found = 0;
    //Probably should check to make sure sector isn't full first
    for (int i = 0; i * sizeof(DIRENTRY) < BootSec.BytesPerSec && found == 0; i++)
    {
        for (int j = 0; j < 16; j += 2)
        {

            //Need to read and check if empty
            lseek(fatFD, CurDataSec + (j * 32), SEEK_SET);
            read(fatFD, &testDir, sizeof(DIRENTRY));
            if (testDir.Name[0] == 0x00 || testDir.Name[0] == 0xE5)
            {
                data_write_location = CurDataSec + (j * 32);
                found = 1;
                break;
            }
        }
    }
    if (found == 1)
    {
        //Hopefully this works
        lseek(fatFD, data_write_location, SEEK_SET);
        write(fatFD, &dirEntry, sizeof(DIRENTRY));
    }
    else
    {
        printf("Directory Full\n");
        return -1;
    }

    if (isDirectory)
    {
        DIRENTRY dotDirectory;
        DIRENTRY dot2Directory;
        unsigned int new_loc = getDataSecForClus(fat_cluster);
        createNewEntry(&dot2Directory, isDirectory, CurClus, "..");
        createNewEntry(&dotDirectory, isDirectory, fat_cluster, ".");

        lseek(fatFD, new_loc, SEEK_SET);
        write(fatFD, &dotDirectory, sizeof(DIRENTRY));

        lseek(fatFD, new_loc + 32, SEEK_SET);
        write(fatFD, &dot2Directory, sizeof(DIRENTRY));
    }
}

void ls(tokenlist *tokens)
{
    unsigned int dataSec = CurDataSec;
    if (tokens->size > 1)
    {
        //search for directory matching DIRNAME
        char *dirname = tokens->items[1];
        if (strcmp(dirname, ".") != 0)
        {
            int clusNum = searchForDirClusNum(dirname);
            if (clusNum >= 0)
                dataSec = getDataSecForClus(clusNum);
            else
                return;
        }
    }

    //lseek sets read pointer
    lseek(fatFD, dataSec, SEEK_SET);

    DIRENTRY dirEntry;
    for (int i = 0; i * sizeof(DIRENTRY) < BootSec.BytesPerSec; i++)
    {

        //Move Data to DirEntry
        read(fatFD, &dirEntry, sizeof(DIRENTRY));

        //32 are files, 16 are folders
        if (strlen(dirEntry.Name) > 0 && (dirEntry.Attr == 32 || dirEntry.Attr == 16))
        {
            printf("%s\n", dirEntry.Name);
            // printf("Type: %i\n", dirEntry.Attr);
        }
        // printf("Directory Offset %i\n", CurDataSec);
        // printf("Size: %i\n", dirEntry.FileSize);
    }
}

int HiLoClusConvert(unsigned short HI, unsigned short LO)
{
    int res = LO;
    if (HI != 0)
        res += (HI << 16);
    if (res == 0)
        res = BootSec.RootClus;
    return res;
}

void cd(tokenlist *tokens)
{
    if (tokens->size > 1)
    {
        //search for directory matching DIRNAME
        char *dirname = tokens->items[1];
        if (strcmp(dirname, ".") == 0)
        {
            return;
        }

        int clus = searchForDirClusNum(dirname);
        if (clus >= 0)
        {
            CurClus = searchForDirClusNum(dirname);
            CurDataSec = getDataSecForClus(CurClus);
        }
    }
}

tokenlist *new_tokenlist(void)
{
    tokenlist *tokens = (tokenlist *)malloc(sizeof(tokenlist));
    tokens->size = 0;
    tokens->items = NULL;
    return tokens;
}

tokenlist *get_tokens(char *input, char *delims)
{
    char *buf = (char *)malloc(strlen(input) + 1);
    strcpy(buf, input);

    tokenlist *tokens = new_tokenlist();

    char *tok = strtok(buf, delims);
    while (tok != NULL)
    {
        add_token(tokens, tok);
        tok = strtok(NULL, delims);
    }

    tokens->items = (char **)realloc(tokens->items, (tokens->size + 1) * sizeof(char *));
    tokens->items[tokens->size] = NULL;

    free(buf);
    return tokens;
}

char *get_input(void)
{
    char *buffer = NULL;
    int bufsize = 0;

    char line[5];
    while (fgets(line, 5, stdin) != NULL)
    {
        int addby = 0;
        char *newln = strchr(line, '\n');
        if (newln != NULL)
            addby = newln - line;
        else
            addby = 5 - 1;

        buffer = (char *)realloc(buffer, bufsize + addby);
        memcpy(&buffer[bufsize], line, addby);
        bufsize += addby;

        if (newln != NULL)
            break;
    }

    buffer = (char *)realloc(buffer, bufsize + 1);
    buffer[bufsize] = 0;

    return buffer;
}

void add_token(tokenlist *tokens, char *item)
{
    if (tokens != NULL)
    {
        int i = tokens->size;
        tokens->items = (char **)realloc(tokens->items, (i + 1) * sizeof(char *));
        tokens->items[i] = (char *)malloc(strlen(item) + 1);
        strcpy(tokens->items[i], item);
        tokens->size += 1;
    }
}

void free_tokens(tokenlist *tokens)
{
    for (int i = 0; i < tokens->size; i++)
        free(tokens->items[i]);

    free(tokens);
}

void trimStringRight(char *str)
{
    int index, i;
    index = -1;

    i = 0;
    while (str[i] != '\0')
    {
        if (str[i] != ' ' && str[i] != '\t' && str[i] != '\n')
        {
            index = i;
        }

        i++;
    }
    str[index + 1] = '\0';
}

char *padRight(char *string, int padded_len, char *pad)
{
    char *z = " "; //one ASCII zero
    char *str = (char *)malloc(padded_len * sizeof(char) + 1);

    strcpy(str, string);
    while (strlen(str) < padded_len)
    {
        strcat(str, z);
    }
    return str;
}
int getDataSecForClus(int N)
{
    return (BootSec.RsvdSecCnt + N - BootSec.RootClus + (BootSec.NumFATs * BootSec.FATSz32)) * BootSec.BytesPerSec;
}

int searchForDirClusNum(char *dirname)
{
    tokenlist *dirTokens = get_tokens(dirname, "/");
    return searchForDirClusNum_H(dirTokens, 0, CurClus);
}

int searchForDirClusNum_H(tokenlist *dirTokens, int curIdx, int cluster)
{
    if (curIdx >= dirTokens->size)
    {
        return cluster;
    }
    else
    {
        char *dirname = dirTokens->items[curIdx];
        DIRENTRY dirEntry;
        int nextClus;
        int dataSec = getDataSecForClus(cluster);
        lseek(fatFD, dataSec, SEEK_SET);
        for (int i = 0; i * sizeof(DIRENTRY) < BootSec.BytesPerSec; i++)
        {
            //Move Data to DirEntry
            read(fatFD, &dirEntry, sizeof(DIRENTRY));

            //32 are files, 16 are folders
            if ((strlen(dirEntry.Name) > 0) && (dirEntry.Attr == 16 || dirEntry.Attr == 32))
            {
                if (strncmp(dirEntry.Name, dirname, strlen(dirname)) == 0)
                {
                    if (dirEntry.Attr == 16)
                    {
                        nextClus = HiLoClusConvert(dirEntry.FstClusHI, dirEntry.FstClusLO);
                        return searchForDirClusNum_H(dirTokens, ++curIdx, nextClus);
                    }
                    else
                    {
                        printf("error: %s is not a directory\n", dirname);
                        return -1;
                    }
                }
            }
        }
        printf("error: %s does not exist\n", dirname);
        return -1;
    }
}

void createNewEntry(DIRENTRY *entry, int isDirectory, unsigned int address, char *filename)
{
    const char *name = padRight(filename, 8, ' ');
    strcpy(entry->Name, name);
    if (isDirectory)
        entry->Attr = 16;
    else
        entry->Attr = 32;
    entry->NTRes = 0;
    entry->CrtTimeTenth = 0;
    entry->CrtTime = 0;
    entry->CrtDate = 0;
    entry->LstAccDate = 0;
    entry->FstClusHI = address >> 16;
    entry->WrtTime = 0;
    entry->WrtDate = 0;
    entry->FstClusLO = address;
    entry->FileSize = 0;
}

void rm(char *filename)
{
    unsigned int EMPTY_DIRECTORY = 0x00000000;

    //STEP 1 check if file is in directory and erase
    DIRENTRY dirEntry;
    lseek(fatFD, CurDataSec, SEEK_SET);
    bool found = false;
    int filesize = dirEntry.FileSize;
    for (int i = 0; i * sizeof(DIRENTRY) < BootSec.BytesPerSec; i++)
    {
        //Move Data to DirEntry
        read(fatFD, &dirEntry, sizeof(DIRENTRY));
        if (strncmp(dirEntry.Name, filename, strlen(filename)) == 0)
        {
            lseek(fatFD, -32, SEEK_CUR);        //Seek 32 bytes back
            write(fatFD, &EMPTY_DIRECTORY, 32); //Write empty directory
            found = true;
            break;
        }
    }


    if (!found)
    {
        printf("File Not found\n");
        return;
    }

    if(dirEntry.Attr == 16){
        printf("Cannot rm a directory, we may implement `rm -r <dir>` in the future\n");
        return;
    }

    //STEP 2 Clean out fat
    unsigned int fat_cluster = HiLoClusConvert(dirEntry.FstClusHI, dirEntry.FstClusLO);
    unsigned int sector = getDataSecForClus(fat_cluster);
    unsigned int fat_address = clusterToFatAddress(fat_cluster);
    unsigned int fatEntry;

    lseek(fatFD, fat_address, SEEK_SET);
    read(fatFD, &fatEntry, sizeof(fatEntry));
    int delete_size = filesize / 32;
    unsigned int EMPTY_BYTE = 0x00;
    while (filesize > 0)
    {
        lseek(fatFD, sector, SEEK_SET);

        if (delete_size > 1)
        {
            for (int i = 0; i < filesize; i++)
            {
                write(fatFD, &EMPTY_BYTE, 1); //Write empty byte
                filesize -= 1;
            }
        }
        else
        {
            write(fatFD, &EMPTY_DIRECTORY, 32); //Write empty directory
            filesize -= 32;
        }

        //write to fat
        lseek(fatFD, fat_address, SEEK_SET);
        write(fatFD, 0x00, 4); //Mark fat empty

        if (fatEntry == 0x0FFFFFF8 || fatEntry == 0x0FFFFFFF) //Check if at end of fat
            break;

        //Move to next cluster
        sector = getDataSecForClus(fatEntry);
        fat_address = clusterToFatAddress(fatEntry);
        lseek(fatFD, fat_address, SEEK_SET);
        read(fatFD, &fatEntry, sizeof(fatEntry));
    }
}

unsigned int clusterToFatAddress(unsigned int clusterNum)
{

    return (((clusterNum * 4) / BootSec.BytesPerSec) + BootSec.RsvdSecCnt) * BootSec.BytesPerSec + ((clusterNum * 4) % BootSec.BytesPerSec);
}

