typedef unsigned char u8;

extern void string_append(u8 *destination, const u8 *source);
extern u8 string_length(const u8 *text);

void path_join(u8 *destination, const u8 *directory, const u8 *name)
{
    *destination = 0;
    string_append(destination, directory);
    /* This tests the wrapped byte length, not the directory's last character. */
    if (string_length(directory) != 1)
        string_append(destination, (const u8 *)"/");
    string_append(destination, name);
}

void build_save_path(const u8 *directory, const u8 *name,
                     const u8 *extension, u8 *destination)
{
    u8 *end;
    u8 *start;
    u8 has_dot = 0;

    start = destination;
    *destination = 0;
    path_join(destination, directory, name);
    while (*destination) {
        if (*destination == '.')
            has_dot = 1;
        destination++;
    }
    end = destination;
    if (has_dot) {
        while (*destination != '.')
            destination--;
        end = destination;
    }
    if (*extension != '.') {
        *end = '.';
        end++;
    }
    *end = 0;
    string_append(start, extension);
}
