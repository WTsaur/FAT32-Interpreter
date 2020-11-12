#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "BPB.h"
#include "DIRENTRY.h"

void printInfo(BPB* bpbInfo);
void ls(unsigned int dirEntryOffset, int fatFD, BPB* bpbInfo);
void cd(unsigned int dirEntryOffset, int fatFD, BPB* bpbInfo);

typedef struct {
	int size;
	char** items;
} tokenlist;

tokenlist *new_tokenlist(void);
tokenlist *get_tokens(char *input, char *delims);
char *get_input(void);
void add_token(tokenlist *tokens, char *item);
void free_tokens(tokenlist *tokens);

int main(int argc, char* argv[]) {
    char *fatFile;
    int fatFD = -1;
    BPB bootSec;

    if (argc != 2) {
        printf("error: usage: %s <FAT32 File>\n", argv[0]);
        exit(1);
    }

    fatFile = argv[1];
    fatFD = open(fatFile, O_RDWR);
    if (fatFD == -1) {
        printf("error opening FAT32 image file: %s\n", fatFile);
    }

    /* Retrieve BPB Info */
    unsigned char* buf;
    buf = (unsigned char*) malloc(512);
    if (read(fatFD, buf, 512) == -1) {
        printf("error reading boot sector");
    }
    memcpy(&bootSec, buf, sizeof(BPB));

    //This stores the currrent directory. Intialize to root. (ReserveCount+(#Fat * FatSize)) * bytesPerSector
    unsigned int currentClusterDirectory = (bootSec.RsvdSecCnt + (bootSec.NumFATs * bootSec.FATSz32)) * bootSec.BytesPerSec;



    char *command = (char*) malloc(100);
    command = " ";
    while (strcmp(command, "exit") != 0) {
        printf("$ ");
		char *input = get_input();
		tokenlist *tokens = get_tokens(input, " ");

		if (tokens->size == 0) continue;

        // char *arg = (char*) malloc(100);

        // char* input = (char*) malloc(100);
        // char** argList;
        // const char* delim = " ";
        command = tokens->items[0];
        if (strcmp(command, "info") == 0) {
            printInfo(&bootSec);
        } else if (strcmp(command, "size") == 0) {
            //size();
        } else if (strcmp(command, "ls") == 0) {
            ls(currentClusterDirectory, fatFD, &bootSec);
        } else if (strcmp(command, "cd") == 0) {
            //cd();
        } else if (strcmp(command, "creat") == 0) {
            //creat();
        } else if (strcmp(command, "mkdir") == 0) {
            //mkdir();
        } else if (strcmp(command, "mv") == 0) {
            //mv();
        } else if (strcmp(command, "open") == 0) {
            //open();
        } else if (strcmp(command, "close") == 0) {
            //close();
        } else if (strcmp(command, "lseek") == 0) {
            //lseek();
        } else if (strcmp(command, "read") == 0) {
            //read();
        } else if (strcmp(command, "write") == 0) {
            //write();
        } else if (strcmp(command, "rm") == 0) {
            //rm();
        } else if (strcmp(command, "cp") == 0) {
            //cp();
        } else if (strcmp(command, "rmdir") == 0) {
            //rmdir();
        } else {
            printf("invalid command: %s\n", command);      
        }
    }

    exit(0);
}

void printInfo(BPB* bpbInfo) {
    printf("Bytes Per Sector: %i\n", (*bpbInfo).BytesPerSec);
    printf("Sectors Per Cluser: %i\n", (*bpbInfo).SecPerClus);
    printf("Reserved Sector Count: %i\n", (*bpbInfo).RsvdSecCnt);
    printf("Number of FATs: %i\n", (*bpbInfo).NumFATs);
    printf("Total Sectors: %i\n", (*bpbInfo).TotSec32);
    printf("FAT Size: %i\n", (*bpbInfo).FATSz32);
    printf("Root Cluster: 0x%.2X\n", (*bpbInfo).RootClus);
}

void ls(unsigned int currentClusterDirectory, int fatFD, BPB* bpbInfo){
    //Lseek sets read pointer
    lseek(fatFD,currentClusterDirectory, SEEK_SET);

    DIRENTRY dirEntry;
	for(int i=0; i*sizeof(DIRENTRY) < (*bpbInfo).BytesPerSec; i++){

    //Move Data to DirEntry
    read(fatFD, &dirEntry, sizeof(DIRENTRY));

    if(strlen(dirEntry.Name) > 0){
        printf("%s\n", dirEntry.Name);
             printf("Attr: %i\n", dirEntry.Attr);


    }
    // printf("Directory Offset %i\n", currentClusterDirectory);
    // printf("Size: %i\n", dirEntry.FileSize);
    }

}

void cd(unsigned int currentClusterDirectory, int fatFD, BPB* bpbInfo){
    
}

tokenlist* new_tokenlist(void)
{
	tokenlist* tokens = (tokenlist*)malloc(sizeof(tokenlist));
	tokens->size = 0;
	tokens->items = NULL;
	return tokens;
}

tokenlist *get_tokens(char *input, char* delims)
{
	char *buf = (char *) malloc(strlen(input) + 1);
	strcpy(buf, input);

	tokenlist *tokens = new_tokenlist();

	char *tok = strtok(buf, delims);
	while (tok != NULL) {
		add_token(tokens, tok);
		tok = strtok(NULL, delims);
	}

	tokens->items = (char**)realloc(tokens->items, (tokens->size + 1)*sizeof(char*));
	tokens->items[tokens->size] = NULL;

	free(buf);
	return tokens;
}

char* get_input(void)
{
	char* buffer = NULL;
	int bufsize = 0;

	char line[5];
	while (fgets(line, 5, stdin) != NULL) {
		int addby = 0;
		char* newln = strchr(line, '\n');
		if (newln != NULL)
			addby = newln - line;
		else
			addby = 5 - 1;

		buffer = (char*)realloc(buffer, bufsize + addby);
		memcpy(&buffer[bufsize], line, addby);
		bufsize += addby;

		if (newln != NULL)
			break;
	}

	buffer = (char*)realloc(buffer, bufsize + 1);
	buffer[bufsize] = 0;

	return buffer;
}

void add_token(tokenlist *tokens, char *item)
{
    if (tokens != NULL) {
        int i = tokens->size;
	    tokens->items = (char **) realloc(tokens->items, (i + 1) * sizeof(char *));
	    tokens->items[i] = (char *) malloc(strlen(item) + 1);
        strcpy(tokens->items[i], item);
	    tokens->size += 1;
    }
}

void free_tokens(tokenlist* tokens)
{
	for (int i = 0; i < tokens->size; i++)
		free(tokens->items[i]);

	free(tokens);
}
