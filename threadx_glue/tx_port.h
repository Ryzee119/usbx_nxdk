#ifndef TX_PORT_H
#define TX_PORT_H

#ifdef TX_INCLUDE_USER_DEFINE_FILE
#include "tx_user.h"
#endif

#define TX_MAX_PRIORITIES 32
#define TX_MINIMUM_STACK 1

#define TX_MEMSET(a, b, c)          \
    {                               \
        UCHAR *ptr;                 \
        UCHAR value;                \
        UINT i, size;               \
        ptr = (UCHAR *)((VOID *)a); \
        value = (UCHAR)b;           \
        size = (UINT)c;             \
        for (i = 0; i < size; i++)  \
        {                           \
            *ptr++ = value;         \
        }                           \
    }

#define ULONG64 unsigned long long
#define ULONG64_DEFINED

#endif
