#include <stdio.h>
const void *ptrprintf = printf;
#pragma section(".exre", execute, read)
__declspec(allocate(".exre")) int main[] =
{
    0x646C6890, 0x20680021, 0x68726F57,
    0x2C6F6C6C, 0x48000068, 0x24448D65,
    0x15FF5002, &ptrprintf, 0xC314C483
};
