/* peek ADDR LEN: hex dump of memory (hex ADDR, decimal LEN), 16 bytes a line, to stdout. For
 * finding which loaded module a recorded caller address belongs to (search the bytes in the
 * program's files on the host). Build: m68k-amigaos-gcc -noixemul -Os -s peek.c -o peek */
#include <stdio.h>
#include <stdlib.h>
#include <exec/types.h>

int main(int argc, char **argv)
{
    ULONG a, n, i;
    if (argc < 3) { printf("peek ADDR(hex) LEN\n"); return 5; }
    a = strtoul(argv[1], NULL, 16); n = strtoul(argv[2], NULL, 10);
    for (i = 0; i < n; i++) {
        if ((i & 15) == 0) printf("%08lx:", a + i);
        printf(" %02x", *(UBYTE *)(a + i));
        if ((i & 15) == 15) printf("\n");
    }
    if (n & 15) printf("\n");
    return 0;
}
