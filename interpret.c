#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {
    char *fatFile;

    if (argc != 2) {
        printf("error: usage: %s <FAT32 File>\n", argv[0]);
        exit(1);
    }

    fatFile = argv[1];
    
    exit(0);
}