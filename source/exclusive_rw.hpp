#pragma once
#include <3ds.h>
static void inline ExclusiveWrite32(s32 *addr, s32 val)
{
    do
        __ldrex(addr);
    while ( __strex(addr, val));
}

static void inline ExclusiveWrite16(u16 *addr, u16 val)
{
    do
        __ldrexh(addr);
    while ( __strexh(addr, val));
}