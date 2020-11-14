#ifndef DIRENTRY_H
#define DIRENTRY_H

struct DIRENTRY_struct {
    char Name[11];
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
} __attribute((packed));

typedef DIRENTRY_struct DIRENTRY;

#endif
