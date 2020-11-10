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

int main(int argc, char* argv[]) {
    char *fatFile;
    char command[100] = "";
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

    while (strcmp(command, "exit") != 0) {
        printf("$ ");
        scanf("%s", command);
        while ((getchar()) != '\n');
        if (strcmp(command, "info") == 0) {
            printInfo(&bootSec);
        } else if (strcmp(command, "size") == 0) {
            //size();
        } else if (strcmp(command, "ls") == 0) {
            //ls();
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
        } else if (strcmp(command, "exit") == 0) {
            //exit();
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