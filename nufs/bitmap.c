// bitmap_print adapted from print_byte_as_bits method at:
// https://jameshfisher.com/2017/02/23/printing-bits/

#include <stdio.h>
#include "bitmap.h"

int bitmap_get(char *bm, int ii)
{
    int i = ii / 8;
    int off = ii % 8;
    char flag = 1 << off;

    return bm[i] & flag;
}

void bitmap_put(char *bm, int ii, int vv)
{
    int i = ii / 8;
    int off = ii % 8;
    char flag = 1 << off;

    if (vv)
    {
        bm[i] = bm[i] | flag;
    }
    else
    {
        flag = ~flag;
        bm[i] = bm[i] & flag;
    }
}

void bitmap_print(char *bm, int size)
{
    // for (int ii = 31; ii >= 0; ii--) {
    //     (val & (1 << ii)) ? '1' : '0');
    // }
}
