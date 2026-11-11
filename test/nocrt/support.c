// Everything the library links against besides Windows' import libraries.
// The compiler turns CopyMemory, MoveMemory, ZeroMemory, structure copies
// and initializers into calls to these three; nothing else is needed on x86,
// x64 or ARM64, with what CMakeLists.txt switches off. A function the
// library starts to call from the C runtime stays unresolved here, which
// fails the link.
//
// ntdll.lib would supply all three too, but it exports most of the C string
// functions as well, so a new dependency on one would link and hide.
//
// Each body is a plain loop kept out of the optimizer, which would otherwise
// recognize it and call the function it is defining.
// Only for the prototypes the compiler wants before it takes the definitions
// as the intrinsics' replacements.
#include <string.h>

#ifdef __clang__
#define NOCRT_NO_OPTIMIZE __attribute__((optnone))
#else
#define NOCRT_NO_OPTIMIZE
#pragma optimize("", off)
#pragma function(memcpy, memmove, memset)
#endif

NOCRT_NO_OPTIMIZE void* memcpy(void* pvDest, const void* pvSource, size_t cb)
{
    unsigned char* pbDest = (unsigned char*)pvDest;
    const unsigned char* pbSource = (const unsigned char*)pvSource;
    while (cb--)
        *pbDest++ = *pbSource++;
    return pvDest;
}

NOCRT_NO_OPTIMIZE void* memmove(void* pvDest, const void* pvSource, size_t cb)
{
    unsigned char* pbDest = (unsigned char*)pvDest;
    const unsigned char* pbSource = (const unsigned char*)pvSource;
    if (pbDest <= pbSource || pbDest >= pbSource + cb)
    {
        while (cb--)
            *pbDest++ = *pbSource++;
    }
    else
    {
        while (cb--)
            pbDest[cb] = pbSource[cb];
    }
    return pvDest;
}

NOCRT_NO_OPTIMIZE void* memset(void* pvDest, int value, size_t cb)
{
    unsigned char* pbDest = (unsigned char*)pvDest;
    while (cb--)
        *pbDest++ = (unsigned char)value;
    return pvDest;
}
