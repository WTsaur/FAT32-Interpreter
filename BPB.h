#ifndef BPB_H
#define BPB_H

struct BPB_struct {
    unsigned char BS_jmpBoot[3];
    unsigned char BS_OEMName[8];
    unsigned short BytesPerSec;
    unsigned char SecPerClus;
    unsigned short RsvdSecCnt;
    unsigned char NumFATs;
    unsigned short RootEntCnt;
    unsigned short TotSec16;
    unsigned char Media;
    unsigned short FATSz16;
    unsigned short SecPerTrk;
    unsigned short NumHeads;
    unsigned int HiddSec;
    unsigned int TotSec32;
    unsigned int FATSz32;
    unsigned short ExtFlags;
    unsigned short FSVer;
    unsigned int RootClus;
    unsigned short FSInfo;
    unsigned short BkBootSec;
    unsigned char Reserved[12];
    unsigned char DrvNum;
    unsigned char Reserved1;
    unsigned char Bootsig;
    unsigned int VolID;
    unsigned char VolLab[11];
    unsigned char FilSysType[8];
    unsigned char CodeReserved[420];
    unsigned short Signature_word;
}__attribute((packed));

typedef BPB_struct BPB;

#endif