#ifndef DIRENTRY_H
#define DIRENTRY_H

typedef struct {
    unsigned char Name[11];
    unsigned char Attr;
    unsigned char NTRes;
    unsigned char CrtTimeTenth;
    unsigned short CrtTime;
    unsigned short CrtDate;
    unsigned short LstAccDate;
    unsigned short FstClusHI;
    unsigned short WrtTime;
    unsigned short WrtDate;
    unsigned short FstClusLO;
    unsigned int FileSize;
} __attribute((packed)) DIRENTRY;

#endif