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

struct File_Entry
{
    unsigned int first_cluster;
    int mode;
    unsigned int offset;
    struct File_Entry *next;
};

#define READ 0
#define WRITE 1
#define READ_WRITE 2

struct File_Entry *headFileNode = NULL;
struct File_Entry *currFileNode = NULL;
struct File_Entry *nextFileNode = NULL;

void printInfo();
void ls(tokenlist *token, int flag);
void size(tokenlist *tokens);
int cd(char *path, int flag);
void rm(char *filename);
void openFile(tokenlist *tokens);
void closeFile(tokenlist *tokens);
void lseekFAT(tokenlist *tokens);
char *readFAT(tokenlist *tokens, char *filename, int flag);
void writeFAT(tokenlist *tokens, char *filename, char *newText, unsigned int cluster, int flag);
int cp(tokenlist *tokens, int isRemove);
void trimStringRight(char *str);
int HiLoClusConvert(unsigned short HI, unsigned short LO);                          /* converts DIRENTRY's FstClusHi and FstClusLo to a cluster number */
int getDataSecForClus(int N);                                                       /* calculates the data sector for a given cluster, N */
int searchForDirClusNum(char *dirname, unsigned int cluster, int flag);             /* searches cwd for dir and returns the cluster num for that dir */
int searchForDirClusNum_H(tokenlist *dirTokens, int curIdx, int cluster, int flag); /* helper func for searchForDirClusNum */

unsigned int clusterToFatAddress(unsigned int clusterNum); /*  Takes cluster number and returns fat address*/
int create(char *filename, int isDirectory, unsigned int cluster);
void createNewEntry(DIRENTRY *entry, int isDirectory, unsigned int address, char *name); /*Creates empty DIRENTRY object of type file or directory*/

tokenlist *new_tokenlist(void);
tokenlist *get_tokens(char *input, char *delims);
char *get_input(void);
void add_token(tokenlist *tokens, char *item);
void free_tokens(tokenlist *tokens);

int main(int argc, char *argv[])
{
    char *fatFile;
    headFileNode = NULL;
    currFileNode = (struct File_Entry *)malloc(sizeof(struct File_Entry));
    nextFileNode = (struct File_Entry *)malloc(sizeof(struct File_Entry));
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
            if (tokens->size == 2)
                size(tokens);
            else
                printf("error: usage: ls <FILE NAME>\n");
        }
        else if (strcmp(command, "ls") == 0)
        {
            if (tokens->size < 3)
                ls(tokens, 0);
            else
                printf("error: usage: ls <DIR NAME>\n");
        }
        else if (strcmp(command, "cd") == 0)
        {
            if (tokens->size < 3)
                cd(tokens->items[1], 0);
            else
                printf("error: usage: cd <DIR NAME>\n");
        }
        else if (strcmp(command, "creat") == 0)
        {
            if (tokens->size < 2)
                printf("error: usage: creat <FILE NAME>\n");
            else
                create(tokens->items[1], 0, CurClus);
        }
        else if (strcmp(command, "mkdir") == 0)
        {
            if (tokens->size < 2)
                printf("error: usage: mkdir <DIR NAME>\n");
            else
                create(tokens->items[1], 1, CurClus);
        }
        else if (strcmp(command, "mv") == 0)
        {
            if (tokens->size != 3)
            {
                printf("error: usage mv <FROM> <TO>\n");
            }
            else
            {
                cp(tokens, 1);
            }
        }
        else if (strcmp(command, "open") == 0)
        {
            if (tokens->size != 3)
            {
                printf("error: usage: open <FILE NAME> <Mode>\n");
            }
            else
            {
                openFile(tokens);
            }
        }
        else if (strcmp(command, "close") == 0)
        {
            if (tokens->size != 2)
            {
                printf("error: usage: close <FILE NAME>\n");
            }
            else
            {
                closeFile(tokens);
            }
        }
        else if (strcmp(command, "lseek") == 0)
        {
            if (tokens->size != 3)
            {
                printf("error: usage: lseek <FILE NAME> <OFFSET>\n");
            }
            else
            {
                lseekFAT(tokens);
            }
        }
        else if (strcmp(command, "read") == 0)
        {
            if (tokens->size != 3)
            {
                printf("error: usage: read <FILE NAME> <SIZE>\n");
            }
            else
            {
                readFAT(tokens, "", 0);
            }
        }
        else if (strcmp(command, "write") == 0)
        {
            if (tokens->size != 4)
            {
                printf("error: usage: write <FILE NAME> <SIZE> <BUFFER>\n");
            }
            else
            {
                writeFAT(tokens, "", "", CurClus, 0);
            }
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
            if (tokens->size != 3)
            {
                printf("error: usage cp <FILE NAME> <TO>\n");
            }
            else
            {
                cp(tokens, 0);
            }
        }
        else if (strcmp(command, "exit") == 0)
        {
            //exit
            free(buf);
            free_tokens(tokens);
            close(fatFD);
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

int create(char *filename, int isDirectory, unsigned int cluster)
{
    if (strlen(filename) > 11)
    {
        printf("name must not exceed 11 Characters\n");
        return -1;
    }
    unsigned int FREE_CLUSTER = 0x00000000;
    unsigned int FAT_END = 0x0FFFFFF8;

    DIRENTRY dirEntry;
    DIRENTRY testDir;

    //Check if name already exist
    unsigned int working_cluster = cluster;
    unsigned int fat_address;
    while (!(working_cluster == 0x0FFFFFF8 || working_cluster == 0x0FFFFFFF))
    {
        unsigned int dataSec = getDataSecForClus(working_cluster);
        lseek(fatFD, dataSec, SEEK_SET);
        for (int i = 0; i * sizeof(DIRENTRY) < BootSec.BytesPerSec; i++)
        {
            read(fatFD, &dirEntry, sizeof(DIRENTRY));
            if (strncmp(dirEntry.Name, filename, strlen(filename)) == 0)
            {
                printf("file name already exist\n");
                return -1;
            }
        }
        fat_address = clusterToFatAddress(working_cluster);
        lseek(fatFD, fat_address, SEEK_SET);
        read(fatFD, &working_cluster, sizeof(dirEntry));
    }

    //STEP 1 Find free fat entry
    int i = 0;

    // sectors_per_cluster
    unsigned int cluster_count = (BootSec.TotSec32 - (BootSec.RsvdSecCnt + (BootSec.NumFATs * BootSec.FATSz32)) * BootSec.BytesPerSec) / BootSec.SecPerClus;
    unsigned int fat_entry;
    unsigned int data_write_location;
    unsigned int fat_write_address;
    unsigned int fat_cluster = 0;
    int found = 0;

    while (i < cluster_count)
    {

        fat_write_address = clusterToFatAddress(i);
        lseek(fatFD, fat_write_address, SEEK_SET);
        read(fatFD, &fat_entry, sizeof(fat_entry));
        if (fat_entry == FREE_CLUSTER)
        {
            fat_cluster = i;
            found = 1;
            break;
        }
        i++;
    }
    if (found == 0)
    {
        printf("Fat is full\n");
    }

    //STEP 2 WRITE NEW FAT END
    lseek(fatFD, fat_write_address, SEEK_SET);
    write(fatFD, &FAT_END, 4); //write new file end

    //STEP 3 Create New Entry
    createNewEntry(&dirEntry, isDirectory, fat_cluster, filename);

    //STEP 4 WRITE FILE ENTRY at next open data address
    found = 0;
    //Probably should check to make sure sector isn't full first
    unsigned int workClus = cluster;
    int offset = 0;
    for (int i = 0; i * sizeof(DIRENTRY) < BootSec.BytesPerSec && found == 0; i++)
    {
        for (; offset < 16; offset += 2)
        {

            //Need to read and check if empty
            lseek(fatFD, getDataSecForClus(workClus) + (offset * 32), SEEK_SET);
            read(fatFD, &testDir, sizeof(DIRENTRY));
            if (testDir.Name[0] == 0x00 || testDir.Name[0] == 0xE5)
            {
                data_write_location = getDataSecForClus(workClus) + (offset * 32);
                found = 1;
                break;
            }
        }
        if (found == 1)
            break;
        unsigned int fat_address = clusterToFatAddress(workClus);
        lseek(fatFD, fat_address, SEEK_SET);
        read(fatFD, &workClus, sizeof(dirEntry));
    }
    if (found == 1)
    {
        lseek(fatFD, data_write_location, SEEK_SET);
        write(fatFD, &dirEntry, sizeof(DIRENTRY));
    }
    else
    {
        unsigned int fat_address;
        unsigned int working_cluster = cluster;

        unsigned int cluster_cpy = working_cluster;
        //Check if need to go to next cluster
        while (working_cluster != 0x0FFFFFF8 && working_cluster != 0x0FFFFFFF)
        {
            cluster_cpy = working_cluster;
            fat_address = clusterToFatAddress(working_cluster);
            lseek(fatFD, fat_address, SEEK_SET);
            read(fatFD, &working_cluster, sizeof(dirEntry));
        }
        unsigned int cluster_count = (BootSec.TotSec32 - (BootSec.RsvdSecCnt + (BootSec.NumFATs * BootSec.FATSz32)) * BootSec.BytesPerSec) / BootSec.SecPerClus;
        unsigned int fat_entry;
        unsigned int fat_write_address;
        unsigned int fat_cluster = 0;
        unsigned int FREE_CLUSTER = 0x00000000;
        unsigned int FAT_END = 0x0FFFFFF8;
        int i = 0;
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

        //Write new fat end
        lseek(fatFD, fat_write_address, SEEK_SET);
        write(fatFD, &FAT_END, 4);

        lseek(fatFD, clusterToFatAddress(cluster_cpy), SEEK_SET);
        write(fatFD, &fat_cluster, 4);
        create(filename, isDirectory, fat_cluster);
    }

    if (isDirectory)
    {
        DIRENTRY dotDirectory;
        DIRENTRY dot2Directory;
        unsigned int new_loc = getDataSecForClus(fat_cluster);
        createNewEntry(&dot2Directory, isDirectory, cluster, "..");
        createNewEntry(&dotDirectory, isDirectory, fat_cluster, ".");

        lseek(fatFD, new_loc, SEEK_SET);
        write(fatFD, &dotDirectory, sizeof(DIRENTRY));

        lseek(fatFD, new_loc + 32, SEEK_SET);
        write(fatFD, &dot2Directory, sizeof(DIRENTRY));
    }
    return 1;
}
void size(tokenlist *tokens)
{

    char *filename = tokens->items[1];

    DIRENTRY dirEntry;
    unsigned int working_cluster = CurClus;
    unsigned int fat_address;
    while (!(working_cluster == 0x0FFFFFF8 || working_cluster == 0x0FFFFFFF))
    {
        unsigned int dataSec = getDataSecForClus(working_cluster);
        lseek(fatFD, dataSec, SEEK_SET);

        for (int i = 0; i * sizeof(DIRENTRY) < BootSec.BytesPerSec; i++)
        {
            //Move Data to DirEntry
            read(fatFD, &dirEntry, sizeof(DIRENTRY));

            if (strncmp(dirEntry.Name, filename, strlen(filename)) == 0)
            {
                if (strlen(dirEntry.Name) > 0 && dirEntry.Attr == 32)
                {
                    printf("%s\tsize: %d\n", dirEntry.Name, dirEntry.FileSize);
                    return;
                }
                if (dirEntry.Attr == 16)
                {
                    printf("Cannot get size of Directory\n");
                    return;
                }
            }
        }
        fat_address = clusterToFatAddress(working_cluster);
        lseek(fatFD, fat_address, SEEK_SET);
        read(fatFD, &working_cluster, sizeof(dirEntry));
    }
    printf("File Not Found\n");
}

void ls(tokenlist *tokens, int flag)
{
    unsigned int working_cluster = CurClus;
    unsigned int fat_address;
    int found = 0;
    unsigned int dataSec = getDataSecForClus(working_cluster);
    if (tokens->size > 1)
    {
        //search for directory matching DIRNAME
        char *dirname = tokens->items[1];
        if (strcmp(dirname, ".") != 0)

            working_cluster = searchForDirClusNum(dirname, working_cluster, flag);
        if (working_cluster >= 0)
        {
            dataSec = getDataSecForClus(working_cluster);
            found = 1;
        }
        else
        {
            if (found == 0)
            {
                printf("Directory Not Found\n");
                return;
            }
            
        }
    }

    while (!(working_cluster == 0x0FFFFFF8 || working_cluster == 0x0FFFFFFF))
    {
        dataSec = getDataSecForClus(working_cluster);
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
                found = 1;
            }
        }

        fat_address = clusterToFatAddress(working_cluster);
        lseek(fatFD, fat_address, SEEK_SET);
        read(fatFD, &working_cluster, sizeof(dirEntry));
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

int cd(char *path, int flag)
{
    DIRENTRY dirEntry;

    unsigned int working_cluster = CurClus;
    unsigned int fat_address;
    int clus;
    while (!(working_cluster == 0x0FFFFFF8 || working_cluster == 0x0FFFFFFF))
    {
        //search for directory matching DIRNAME
        char *dirname = path;
        if (strcmp(dirname, ".") == 0)
        {
            return 1;
        }

        clus = searchForDirClusNum(dirname, working_cluster, flag);
        if (clus == -2)
        {
            return -1;
        }
        if (clus >= 0)
        {
            CurClus = searchForDirClusNum(dirname, working_cluster, flag);
            CurDataSec = getDataSecForClus(working_cluster);
            return 1;
        }
        fat_address = clusterToFatAddress(working_cluster);
        lseek(fatFD, fat_address, SEEK_SET);
        read(fatFD, &working_cluster, sizeof(dirEntry));
    }
    if (clus < 0)
    {
        if (flag == 0)
            printf("error: %s does not exist\n", path);
        return -1;
    }
    return 1;
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

int searchForDirClusNum(char *dirname, unsigned int cluster, int flag)
{
    tokenlist *dirTokens = get_tokens(dirname, "/");
    return searchForDirClusNum_H(dirTokens, 0, cluster, flag);
}

int searchForDirClusNum_H(tokenlist *dirTokens, int curIdx, int cluster, int flag)
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
                        return searchForDirClusNum_H(dirTokens, ++curIdx, nextClus, flag);
                    }
                    else if (flag == 0)
                    {
                        printf("error: %s is not a directory\n", dirname);
                        return -2;
                    }
                }
            }
        }
        return -1;
    }
}

void createNewEntry(DIRENTRY *entry, int isDirectory, unsigned int address, char *filename)
{
    const char *name = padRight(filename, 11, " ");
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
    int found = 0;
    int filesize = 0;
    unsigned int working_cluster = CurClus;
    unsigned int fat_address;
    while (!(working_cluster == 0x0FFFFFF8 || working_cluster == 0x0FFFFFFF) && found == 0)
    {
        unsigned int dataSec = getDataSecForClus(working_cluster);
        lseek(fatFD, dataSec, SEEK_SET);

        for (int i = 0; i * sizeof(DIRENTRY) < BootSec.BytesPerSec; i++)
        {
            //Move Data to DirEntry
            read(fatFD, &dirEntry, sizeof(DIRENTRY));

            if (strncmp(dirEntry.Name, filename, strlen(filename)) == 0)
            {
                lseek(fatFD, -32, SEEK_CUR);        //Seek 32 bytes back
                write(fatFD, &EMPTY_DIRECTORY, 32); //Write empty directory
                found = 1;
                break;
            }
        }
        if (found == 0)
        {
            fat_address = clusterToFatAddress(working_cluster);
            lseek(fatFD, fat_address, SEEK_SET);
            read(fatFD, &working_cluster, sizeof(dirEntry));
        }
    }

    if (found == 0)
    {
        printf("File Not found\n");
        return;
    }

    if (dirEntry.Attr == 16)
    {
        printf("Cannot rm a directory, we may implement `rm -r <dir>` in the future\n");
        return;
    }
    filesize = dirEntry.FileSize;

    //STEP 2 Clean out fat
    unsigned int fat_cluster = HiLoClusConvert(dirEntry.FstClusHI, dirEntry.FstClusLO);
    unsigned int sector = getDataSecForClus(fat_cluster);

    unsigned int fatEntry;
    fat_address = clusterToFatAddress(fat_cluster);
    lseek(fatFD, fat_address, SEEK_SET);
    read(fatFD, &fatEntry, sizeof(fatEntry));
    int delete_size = filesize / 32;
    unsigned int EMPTY_BYTE = 0x00;
    while (filesize > 0)
    {
        lseek(fatFD, sector, SEEK_SET);

        if (delete_size > 1)
        { //Remove byte by
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
void openFile(tokenlist *tokens)
{
    DIRENTRY dirEntry;
    char *filename = tokens->items[1];
    char *mode = tokens->items[2];
    int mode_val = -1;
    if (strcmp(mode, "r") == 0)
    {
        mode_val = READ;
    }
    else if (strcmp(mode, "w") == 0)
    {
        mode_val = WRITE;
    }
    else if (strcmp(mode, "rw") == 0 || strcmp(mode, "wr") == 0)
    {
        mode_val = READ_WRITE;
    }
    else
    {
        printf("Invalid mode. Supported modes are `r`, `w`, `wr` or `rw`\n");
        return;
    }
    unsigned int working_cluster = CurClus;
    unsigned int fat_address;
    while (!(working_cluster == 0x0FFFFFF8 || working_cluster == 0x0FFFFFFF))
    {
        unsigned int dataSec = getDataSecForClus(working_cluster);
        lseek(fatFD, dataSec, SEEK_SET);
        for (int i = 0; i * sizeof(DIRENTRY) < BootSec.BytesPerSec; i++)
        {
            read(fatFD, &dirEntry, sizeof(DIRENTRY));
            if (strncmp(dirEntry.Name, filename, strlen(filename)) == 0)
            {
                if (dirEntry.Attr == 16)
                {
                    printf("Cannot open a directory\n");
                    return;
                }
                if ((mode_val == 1 || mode_val == 2) && dirEntry.Attr == 1)
                {
                    printf("Cannot open read only file in `%s` mode", mode);
                    return;
                }
                int cluster_num = HiLoClusConvert(dirEntry.FstClusHI, dirEntry.FstClusLO);
                struct File_Entry *n = headFileNode;
                while (n != NULL)
                {
                    if (n->first_cluster == cluster_num)
                    {
                        printf("File is already open\n");
                        return;
                    }
                    n = n->next;
                }

                //All checks have passed and file is found
                struct File_Entry *newFileEntry;
                newFileEntry = (struct File_Entry *)malloc(sizeof(struct File_Entry));
                newFileEntry->first_cluster = cluster_num;
                newFileEntry->mode = mode_val;
                newFileEntry->offset = 0;
                newFileEntry->next = NULL;
                if (headFileNode == NULL)
                {
                    headFileNode = (struct File_Entry *)malloc(sizeof(struct File_Entry));
                    headFileNode = newFileEntry;
                    currFileNode = newFileEntry;
                }
                else
                {
                    currFileNode->next = newFileEntry;
                    currFileNode = currFileNode->next;
                }
                return;
            }
        }
        fat_address = clusterToFatAddress(working_cluster);
        lseek(fatFD, fat_address, SEEK_SET);
        read(fatFD, &working_cluster, sizeof(dirEntry));
    }
    printf("File not found\n");
}

void closeFile(tokenlist *tokens)
{
    DIRENTRY dirEntry;
    char *filename = tokens->items[1];
    int dataSec = getDataSecForClus(CurClus);
    lseek(fatFD, dataSec, SEEK_SET);
    for (int i = 0; i * sizeof(DIRENTRY) < BootSec.BytesPerSec; i++)
    {
        read(fatFD, &dirEntry, sizeof(DIRENTRY));
        if (strncmp(dirEntry.Name, filename, strlen(filename)) == 0)
        {
            unsigned int cluster_num = HiLoClusConvert(dirEntry.FstClusHI, dirEntry.FstClusLO);

            struct File_Entry *n = headFileNode;
            struct File_Entry *temp = headFileNode;
            if (n != NULL && n->first_cluster == cluster_num)
            {
                if (n->next == NULL)
                {
                    headFileNode = NULL;
                    return;
                }
                headFileNode = n->next;
                free(n);
                return;
            }

            while (n != NULL && n->first_cluster != cluster_num)
            {
                temp = n;
                n = n->next;
            }
            if (n == NULL)
            {
                printf("File Not Open\n");
                return;
            }

            temp->next = n->next;
            free(temp);
            return;
        }
    }
    printf("File Not Open\n");
}

void lseekFAT(tokenlist *tokens)
{
    DIRENTRY dirEntry;
    char *filename = tokens->items[1];
    int offset = atoi(tokens->items[2]);

    unsigned int working_cluster = CurClus;
    unsigned int fat_address;
    while (!(working_cluster == 0x0FFFFFF8 || working_cluster == 0x0FFFFFFF))
    {
        unsigned int dataSec = getDataSecForClus(working_cluster);
        lseek(fatFD, dataSec, SEEK_SET);
        for (int i = 0; i * sizeof(DIRENTRY) < BootSec.BytesPerSec; i++)
        {
            read(fatFD, &dirEntry, sizeof(DIRENTRY));
            if (strncmp(dirEntry.Name, filename, strlen(filename)) == 0)
            {
                unsigned int cluster_num = HiLoClusConvert(dirEntry.FstClusHI, dirEntry.FstClusLO);
                struct File_Entry *n = headFileNode;
                while (n != NULL)
                {
                    if (n->first_cluster == cluster_num)
                    {
                        if (offset > dirEntry.FileSize)
                        {
                            printf("Offset larger then file size\n");
                            return;
                        }
                        n->offset = offset;
                        return;
                    }
                    n = n->next;
                }
            }
        }
        fat_address = clusterToFatAddress(working_cluster);
        lseek(fatFD, fat_address, SEEK_SET);
        read(fatFD, &working_cluster, sizeof(dirEntry));
    }
    printf("File Not Open\n");
}

char *readFAT(tokenlist *tokens, char *filen, int flag)
{

    DIRENTRY dirEntry;
    char *filename;
    int size = 0;
    //Flag 1 = read from input for cp and mv
    //Flag 0 = read from token list
    if (flag == 0)
    {
        filename = tokens->items[1];
        size = atoi(tokens->items[2]);
    }
    else
    {
        filename = filen;
    }

    struct File_Entry *file;
    int found = 0;
    int offset;
    char *bufferText;
    unsigned int working_cluster = CurClus;
    unsigned int fat_address;
    while (!(working_cluster == 0x0FFFFFF8 || working_cluster == 0x0FFFFFFF) && found == 0)
    {
        unsigned int dataSec = getDataSecForClus(working_cluster);
        lseek(fatFD, dataSec, SEEK_SET);
        for (int i = 0; i * sizeof(DIRENTRY) < BootSec.BytesPerSec; i++)
        {
            read(fatFD, &dirEntry, sizeof(DIRENTRY));
            if (strncmp(dirEntry.Name, filename, strlen(filename)) == 0)
            {
                unsigned int cluster_num = HiLoClusConvert(dirEntry.FstClusHI, dirEntry.FstClusLO);
                if (dirEntry.Attr == 16)
                {
                    printf("Cannot Read Directory\n");
                    return "";
                }
                if (flag == 0)
                {
                    struct File_Entry *n = headFileNode;
                    while (n != NULL)
                    {
                        if (n->first_cluster == cluster_num)
                        {
                            if (n->mode == WRITE)
                            {
                                printf("File not in read mode\n");
                                return "";
                            }

                            if (n->offset + size > dirEntry.FileSize)
                            {
                                printf("Offset larger then file size\n");
                                return "";
                            }
                            file = n;
                            offset = file->offset;
                            n->offset = offset + size;
                            found = 1;
                            break;
                        }
                        n = n->next;
                    }
                }
                else if (flag == 1)
                {

                    //Need to find file first cluster
                    found = 1;
                    size = dirEntry.FileSize;
                    offset = 0;
                }
            }
            if (found == 1)
                break;
        }
        if (found == 0)
        {
            fat_address = clusterToFatAddress(working_cluster);
            lseek(fatFD, fat_address, SEEK_SET);
            read(fatFD, &working_cluster, sizeof(dirEntry));
        }
    }
    if (found != 1)
    {
        if (flag == 0)
            printf("File Not Open\n");
        return "";
    }

    unsigned int bytes_remain = size;
    if (flag == 0)
    {
        working_cluster = file->first_cluster;
    }
    else if (flag == 1)
    {
        working_cluster = HiLoClusConvert(dirEntry.FstClusHI, dirEntry.FstClusLO);
        bufferText = (char *)malloc(dirEntry.FileSize);
        bufferText[0] = '\0';
    }

    //set pointer offset
    unsigned int cluster_offset = offset / BootSec.BytesPerSec; //Number of cluster from start
    unsigned int byte_offset = offset % BootSec.BytesPerSec;
    unsigned int cluster_size = BootSec.BytesPerSec * BootSec.SecPerClus;
    while (cluster_offset != 0)
    {

        unsigned int fat_address = clusterToFatAddress(working_cluster);
        lseek(fatFD, fat_address, SEEK_SET);
        read(fatFD, &working_cluster, sizeof(dirEntry));

        cluster_offset--;
    }
    //Traverse and read, switch cluster when needed
    int bytesRead = 0;
    while (bytes_remain > 0)
    {

        if (byte_offset == cluster_size)
        {
            unsigned int fat_address = clusterToFatAddress(working_cluster);
            lseek(fatFD, fat_address, SEEK_SET);
            read(fatFD, &working_cluster, sizeof(dirEntry));
            byte_offset = 0;
            if (working_cluster == 0x0FFFFFF8 || working_cluster == 0x0FFFFFFF)
            {
                break;
            }
        }

        lseek(fatFD, getDataSecForClus(working_cluster) + byte_offset, SEEK_SET);

        if (cluster_size - byte_offset >= bytes_remain)
        {
            char string[bytes_remain];
            read(fatFD, &string, bytes_remain);
            string[bytes_remain] = '\0';
            bytesRead += bytes_remain;
            bytes_remain -= bytes_remain;
            if (flag == 0)
            {
                printf("%s", string);
            }
            else
            {
                strcat(bufferText, string);
            }
        }
        else
        {
            char string[cluster_size - byte_offset];
            read(fatFD, &string, cluster_size - byte_offset);
            string[cluster_size - byte_offset] = '\0';
            bytesRead += cluster_size - byte_offset;
            bytes_remain -= cluster_size - byte_offset;
            if (flag == 0)
            {
                printf("%s", string);
            }
            else
            {
                strcat(bufferText, string);
            }
        }

        byte_offset = 0;
        unsigned int fat_address = clusterToFatAddress(working_cluster);
        lseek(fatFD, fat_address, SEEK_SET);
        unsigned int bytes_remain_buffer = bytes_remain;
        read(fatFD, &working_cluster, sizeof(dirEntry));
        bytes_remain = bytes_remain_buffer;
    }

    if (flag == 1)
    {
        bufferText[bytesRead + 1] = '\0';
        return bufferText;
    }
    else
    {
        return "";
    }
}

void writeFAT(tokenlist *tokens, char *filen, char *newText, unsigned int cluster, int flag)
{
    DIRENTRY dirEntry;

    char *filename;
    int size = 0;
    //Flag 1 = read from input for cp and mv
    //Flag 0 = read from token list
    if (flag == 0)
    {
        filename = tokens->items[1];
    }

    if (flag == 1)
    {
        filename = filen;
    }
    char *buffer;
    if (flag == 0)
    {
        buffer = tokens->items[3];
    }
    else if (flag == 1)
    {
        buffer = newText;
    }
    unsigned int fileSize;
    struct File_Entry *file;

    int found = 0;
    int offset = 0;
    unsigned int cluster_num;

    unsigned int working_cluster = cluster;
    unsigned int fat_address;
    while (!(working_cluster == 0x0FFFFFF8 || working_cluster == 0x0FFFFFFF) && found == 0)
    {
        unsigned int dataSec = getDataSecForClus(working_cluster);
        lseek(fatFD, dataSec, SEEK_SET);
        for (int i = 0; i * sizeof(DIRENTRY) < BootSec.BytesPerSec; i++)
        {
            read(fatFD, &dirEntry, sizeof(DIRENTRY));
            if (strncmp(dirEntry.Name, filename, strlen(filename)) == 0)
            {
                cluster_num = HiLoClusConvert(dirEntry.FstClusHI, dirEntry.FstClusLO);

                if (dirEntry.Attr == 16)
                {
                    printf("Cannot Read Directory\n");
                    return;
                }
                if (flag == 0)
                {
                    size = atoi(tokens->items[2]);

                    struct File_Entry *n = headFileNode;
                    while (n != NULL)
                    {
                        if (n->first_cluster == cluster_num)
                        {
                            if (n->mode == READ)
                            {
                                printf("File not in write mode\n");
                                return;
                            }
                            file = n;

                            if (n->offset + size > dirEntry.FileSize)
                            {
                                //Expand Size
                                unsigned int newSize = file->offset + size;
                                fileSize = newSize;
                                lseek(fatFD, -4, SEEK_CUR);
                                write(fatFD, &newSize, 4);
                            }
                            if (fileSize == 0)
                                fileSize = dirEntry.FileSize;
                            offset = file->offset;
                            n->offset = n->offset + size;
                            found = 1;
                            break;
                        }
                        n = n->next;
                    }
                }
                else if (flag == 1)
                {
                    size = strlen(newText);

                    lseek(fatFD, -4, SEEK_CUR);
                    write(fatFD, &size, 4);
                    //Need to find file first cluster

                    found = 1;
                    offset = 0;
                    break;
                }
            }
            if (found == 1)
                break;
        }

        if (found == 0)
        {
            fat_address = clusterToFatAddress(working_cluster);
            lseek(fatFD, fat_address, SEEK_SET);
            read(fatFD, &working_cluster, sizeof(dirEntry));
        }
    }
    if (found != 1 && flag == 0)
    {
        printf("File Not Open\n");
        return;
    }

    if (flag == 0)
    {
        working_cluster = file->first_cluster;
    }
    else if (flag == 1)
    {
        working_cluster = HiLoClusConvert(dirEntry.FstClusHI, dirEntry.FstClusLO);
    }
    //set pointer offset
    unsigned int cluster_offset = offset / BootSec.BytesPerSec; //Number of cluster from start
    unsigned int byte_offset = offset % BootSec.BytesPerSec;
    unsigned int cluster_size = BootSec.BytesPerSec * BootSec.SecPerClus;

    while (cluster_offset != 0)
    {
        unsigned int fat_address = clusterToFatAddress(working_cluster);
        lseek(fatFD, fat_address, SEEK_SET);
        read(fatFD, &working_cluster, sizeof(dirEntry));

        cluster_offset--;
    }

    int bytesWrite = 0;
    char *buffer_expand = (char *)malloc(size);

    if (size > strlen(buffer))
    {
        memcpy(buffer_expand, buffer, strlen(buffer));
        int cnt = strlen(buffer) + 1;
        //Set remaing as null characters
        while (cnt < size)
        {
            buffer_expand[cnt] = '\0';
            cnt++;
        }
    }
    else if (strlen(buffer) > size)
    {
        memcpy(buffer_expand, buffer, size);
    }
    else
    {
        strcpy(buffer_expand, buffer);
    }
    buffer_expand[size] = '\0';

    int bytes_remain = size;
    while (bytes_remain > 0)
    {
        unsigned int dataSec = getDataSecForClus(working_cluster) + byte_offset;
        lseek(fatFD, dataSec + byte_offset, SEEK_SET);

        if (cluster_size - byte_offset >= bytes_remain)
        {
            write(fatFD, &buffer_expand[bytesWrite], bytes_remain);
            bytes_remain -= bytes_remain;
            bytesWrite += bytes_remain;
        }
        else
        {
            write(fatFD, &buffer_expand[bytesWrite], cluster_size - offset);
            bytes_remain -= cluster_size - offset;
            bytesWrite += cluster_size - offset;
        }

        byte_offset = 0;
        unsigned int fat_address = clusterToFatAddress(working_cluster);
        lseek(fatFD, fat_address, SEEK_SET);
        read(fatFD, &working_cluster, sizeof(dirEntry));
        if (!(working_cluster == 0x0FFFFFF8 || working_cluster == 0x0FFFFFFF) && bytes_remain > 0)
        {
            unsigned int cluster_cpy = working_cluster;
            //Check if need to go to next cluster
            while (!(working_cluster == 0x0FFFFFF8 || working_cluster == 0x0FFFFFFF))
            {
                cluster_cpy = working_cluster;
                fat_address = clusterToFatAddress(working_cluster);
                lseek(fatFD, fat_address, SEEK_SET);
                read(fatFD, &working_cluster, sizeof(dirEntry));
            }
            unsigned int cluster_count = (BootSec.TotSec32 - (BootSec.RsvdSecCnt + (BootSec.NumFATs * BootSec.FATSz32)) * BootSec.BytesPerSec) / BootSec.SecPerClus;
            unsigned int fat_entry;
            unsigned int fat_write_address;
            unsigned int FREE_CLUSTER = 0x00000000;
            unsigned int FAT_END = 0x0FFFFFF8;
            int i = 0;
            while (i < cluster_count)
            {
                fat_write_address = clusterToFatAddress(i);
                lseek(fatFD, fat_write_address, SEEK_SET);
                read(fatFD, &fat_entry, sizeof(fat_entry));
                if (fat_entry == FREE_CLUSTER)
                {
                    break;
                }
                i++;
            }

            //Write new fat end
            lseek(fatFD, fat_write_address, SEEK_SET);
            write(fatFD, &FAT_END, 4);

            lseek(fatFD, clusterToFatAddress(cluster_cpy), SEEK_SET);
            write(fatFD, &cluster_cpy, 4);
        }
    }
}

int cp(tokenlist *tokens, int isRemove)
{
    char *filename = tokens->items[1];
    char *desination = tokens->items[2];
    //Parse desination cluster
    tokenlist *pathList = get_tokens(desination, "/");

    unsigned int tmpCluster = CurClus;
    int isPath = 0;
    for (int i = 0; i < pathList->size; i++)
    {
        int res = cd(pathList->items[i], 1);
        isPath++;
        if (strcmp(pathList->items[i], ".") == 0 || strcmp(pathList->items[i], "..") == 0)
        {
            isPath = 2;
        }

        if (res == -1)
            break;
    }

    unsigned int destinationClust = CurClus;
    CurClus = tmpCluster;

    char *bufferText = readFAT(tokens, filename, 1);
    if (strlen(bufferText) == 0)
    {
        printf("Error: must be a file and not exist in directory To directory, and must exist in current directory\n");
        return -1;
    }
    if (isPath > 1) //Naming Based on if is path or not
    {
        create(filename, 0, destinationClust);
        writeFAT(tokens, filename, bufferText, destinationClust, 1);
    }
    else
    {
        if (CurClus == destinationClust)
        {
            create(desination, 0, destinationClust);
            writeFAT(tokens, desination, bufferText, destinationClust, 1);
        }
        else
        {
            create(filename, 0, destinationClust);
            writeFAT(tokens, filename, bufferText, destinationClust, 1);
        }
    }

    if (isRemove == 1)
    {
        rm(filename);
    }
    return 1;
}
