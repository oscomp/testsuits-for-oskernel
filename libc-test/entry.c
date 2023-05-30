#include "entry.h"


static int mystrcmp(const char *s1, const char *s2) {
    while (*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        return -1;
    }
    int i = 0;
    while(table[i].func) {
        if (mystrcmp(table[i].name, argv[1]) == 0) {
            return table[i].func(argc - 1, argv + 1);
        }
        i++;
    }
    return -1;
}
