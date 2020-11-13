#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "BPB.h"
#include "DIRENTRY.h"

static int fatFD;
static unsigned int CurClus; /* aka cluster number for current working directory */
static BPB BootSec;
static unsigned int CurDataSec;

typedef struct {
    int size;
    char **items;
} tokenlist;

void printInfo();
void ls(tokenlist *tokens);
void cd(tokenlist *tokens);
void trimStringRight(char *str);
int HiLoClusConvert(unsigned short HI, unsigned short LO); /* converts DIRENTRY's FstClusHi and FstClusLo to a cluster number */
int getDataSecForClus(int N); /* calculates the data sector for a given cluster, N */
int searchForDirClusNum(char* dirname); /* searches cwd for dir and returns the cluster num for that dir */

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
    buf = (unsigned char *) malloc(512);
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
            //creat();
        }
        else if (strcmp(command, "mkdir") == 0)
        {
            //mkdir();
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
            //rm();
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

void ls(tokenlist* tokens)
{
    unsigned int dataSec = CurDataSec;
    if (tokens->size > 1) {
        //search for directory matching DIRNAME
        char* dirname = tokens->items[1];
        if (strcmp(dirname, ".") == 0) {
            return;
        }
        int clusNum = searchForDirClusNum(dirname);
        if (clusNum >= 0)
            dataSec = getDataSecForClus(clusNum);
        else
            return;
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

int HiLoClusConvert(unsigned short HI, unsigned short LO) {
    int res = LO;
    if (HI != 0)
        res += (HI << 16);
    if (res == 0)
        res = BootSec.RootClus;
    return res;
}

void cd(tokenlist *tokens) {
    if (tokens->size > 1) {
        //search for directory matching DIRNAME
        char* dirname = tokens->items[1];
        if (strcmp(dirname, ".") == 0) {
            return;
        }
        
        int clus = searchForDirClusNum(dirname);
        if (clus >= 0) {
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

int getDataSecForClus(int N) {
    return (BootSec.RsvdSecCnt + N - BootSec.RootClus + (BootSec.NumFATs * BootSec.FATSz32)) * BootSec.BytesPerSec;
}

int searchForDirClusNum(char* dirname) {
    DIRENTRY dirEntry;
    lseek(fatFD, CurDataSec, SEEK_SET);
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
                    return HiLoClusConvert(dirEntry.FstClusHI, dirEntry.FstClusLO);
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
