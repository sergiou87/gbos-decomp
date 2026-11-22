#ifndef GBOS_FAT_FILE_H
#define GBOS_FAT_FILE_H

typedef struct FatFile {
    u8 name[209];
    u8 padding[3];
    u32 first_entry;
    u32 entry;
    u32 cluster;
    u32 size;
    u8 is_directory;
} FatFile;

typedef struct {
    u32 cluster;
    u32 sector;
    u32 size;
    u16 remaining;
    u8 writing;
    u8 cluster_offset;
} FatFileState;

/* Keep these available without extra include markers in the write-file unit. */
#define STORAGE_SECTOR_SIZE 512
#define FAT_ERR_FILE_MODE2 0xF8
#define FAT_ERR_OUT_OF_FILE_WR 0xFD

#endif
