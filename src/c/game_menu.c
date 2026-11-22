typedef unsigned char u8;
typedef unsigned int u16;
typedef struct FatFile FatFile;

extern u8 menu_select(const u8 **items, u16 selected);
extern u8 program_game(FatFile *file);
extern u8 recover_initialization(void);

u8 load_game(FatFile *file)
{
    const u8 *items[] = {
        (const u8 *)"File Menu", (const u8 *)"Load And Start",
        (const u8 *)"Load Only", 0
    };
    u8 error;

    error = menu_select(items, 0);
    if (error == 0) {
        error = program_game(file);
        if (error)
            return error;
        error = recover_initialization();
        if (error)
            return error;
    } else if (error == 1) {
        error = program_game(file);
        return error;
    }
    return 0;
}
