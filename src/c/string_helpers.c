typedef unsigned char u8;

extern u8 string_length(const u8 *value);
u8 ascii_case_equal(const u8 *left, const u8 *right);

u8 string_has_suffix(const u8 *extension, const u8 *name)
{
    u8 name_length;
    u8 extension_length;

    if (*extension == '.')
        extension++;
    name_length = string_length(name);
    extension_length = string_length(extension);
    if (name_length < extension_length)
        return 0;
    return ascii_case_equal(extension, name + (name_length - extension_length));
}

u8 string_has_any_suffix(const u8 *const *extensions, const u8 *name)
{
    while (*extensions) {
        if (string_has_suffix(*extensions, name))
            return 1;
        extensions++;
    }
    return 0;
}

u8 ascii_case_equal(const u8 *left, const u8 *right)
{
    u8 first;
    u8 second;

    for (;;) {
        first = *left;
        second = *right;
        if (!first && !second)
            return 1;
        left++;
        right++;
        if (first >= 'A' && first <= 'Z')
            first |= 0x20;
        if (second >= 'A' && second <= 'Z')
            second |= 0x20;
        if (first != second)
            return 0;
    }
}

void string_copy(const u8 *source, u8 *destination)
{
    while (*source)
        *destination++ = *source++;
    *destination = 0;
}
