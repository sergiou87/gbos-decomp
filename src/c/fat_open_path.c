typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;
typedef struct FatFile FatFile;

extern u8 fat_find_path(const u8 *path, FatFile *file, u32 *parent, u8 directory);
extern u8 fat_create_path_entry(const u8 *path, u32 parent, u16 sectors, u8 directory);
extern u8 fat_open_file(FatFile *file, u16 sectors);

u8 fat_open_path(const u8 *path, FatFile *file, u16 sectors)
{
    u32 parent;
    u8 error;

    error = fat_find_path(path, file, &parent, 0);
    if (error == 0xf0) {
        if (!sectors)
            return 0xf0;
        if (parent == 0x0fffffff)
            return 0xfa;
        error = fat_create_path_entry(path, parent, sectors, 0);
        if (error)
            return error;
        error = fat_find_path(path, file, &parent, 0);
        if (error)
            return error;
    }
    error = fat_open_file(file, sectors);
    return error;
}
