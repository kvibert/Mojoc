/*
 * Copyright (c) 2012-2019 scott.cgi All Rights Reserved.
 *
 * This code is licensed under the MIT License:
 * https://github.com/scottcgi/Mojoc/blob/master/LICENSE
 *
 * Since : 2013-4-6
 * Update: 2019-1-9
 * Author: scott.cgi
 */

#include <string.h>
#include <Engine/Toolkit/HeaderUtils/ArrayRange.h>
#include "Engine/Toolkit/Utils/BufferReader.h"
#include "Engine/Toolkit/Platform/Log.h"


#define CheckRange(tag)                                            \
    ALog_A                                                        \
    (                                                             \
        range->start <= range->end,                               \
        "ABufferReader " tag " range error start[%d] > end[%d].", \
        range->start,                                             \
        range->end                                                \
    )

#define ReadLineLog() \
    ALog_D("ABufferReader ReadLine = %.*s", outLine->end - outLine->start + 1, buffer + outLine->start)


static int64_t ReadInt64(const char* buffer, ArrayRange* range)
{
    CheckRange("ReadInt64");

    int         pos  = range->start;
    const char* data = buffer + pos;
    range->start    += sizeof(int64_t);

    CheckRange("after ReadInt64");
    
    return (
                ((int64_t)(data[0] & 0xff) << 56) +
                ((int64_t)(data[1] & 0xff) << 48) +
                ((int64_t)(data[2] & 0xff) << 40) +
                ((int64_t)(data[3] & 0xff) << 32) +
                ((int64_t)(data[4] & 0xff) << 24) +
                (         (data[5] & 0xff) << 16) +
                (         (data[6] & 0xff) <<  8) +
                (         (data[7] & 0xff) <<  0)
           );
}


static int32_t ReadInt32(const char* buffer, ArrayRange* range)
{
    CheckRange("ReadInt32");
    
    int         pos  = range->start;
    const char* data = buffer + pos;

    int   ch1        = data[0];
    int   ch2        = data[1];
    int   ch3        = data[2];
    int   ch4        = data[3];
    range->start    += sizeof(int32_t);

    CheckRange("after ReadInt32");

    return ((ch1 << 24) + (ch2 << 16) + (ch3 << 8) + (ch4 << 0));
}


static int16_t ReadInt16(const char* buffer, ArrayRange* range)
{
    CheckRange("ReadInt16");
    
    int         pos  = range->start;
    const char* data = buffer + pos;

    int16_t     ch1  = data[0];
    int16_t     ch2  = data[1];

    range->start    += sizeof(int16_t);

    CheckRange("after ReadInt16");

    return ((ch1 << 8) + (ch2 << 0));
}


static int8_t ReadInt8(const char* buffer, ArrayRange* range)
{
    CheckRange("ReadInt8");
    
    int         pos  = range->start;
    const char* data = buffer + pos;
    range->start    += sizeof(int8_t);

    CheckRange("after ReadInt8");

    return data[0];
}


static void ReadLine(const char* buffer, ArrayRange* range, ArrayRange* outLine)
{
    CheckRange("ReadLine");

    int start      = range->start;
    int end        = range->end;
    outLine->start = start;
    outLine->end   = start;

    for (; start <= end; start++)
    {
        if (buffer[start] == '\n')
        {
            outLine->end = start;

            if (start != end)
            {
                // move to new start
                range->start = start + 1;
            }
            else
            {
                // scan over
                range->start = end;
            }

            ReadLineLog();
            return;
        }
        else if (buffer[start] == '\r')
        {
            if (start != end)
            {
                // check next char
                start++;
                
                if (buffer[start] == '\n')
                {
                    outLine->end = start;

                    if (start != end)
                    {
                        // move to new start
                        range->start = start + 1;
                    }
                    else
                    {
                        // scan over
                        range->start = end;
                    }
                }
                else
                {
                    // back to '\r'
                    outLine->end = start - 1;
                    // start is next
                    range->start = start;
                }
            }
            else
            {
                outLine->end = start;
                // scan over
                range->start = end;
            }

            ReadLineLog();
            return;
        }
    }

    // not found newline char
    outLine->end = end;
    range->end   = end;
    ReadLineLog();
}


static bool TryFindStringByLoop(const char* buffer, ArrayRange* range, const char* str)
{
    CheckRange("TryFindString");

    int  start = range->start;
    int  end   = range->end;
    int  pos   = 0;
    char first  = str[0];
    int  cmp;

    for (; start <= end; start++)
    {
        // always test from first char
        if (buffer[start] == first)
        {
            // compare from start
            cmp = start;

            while (true)
            {
                pos++;

                if (str[pos] == '\0')
                {
                    if (cmp != end)
                    {
                        // move to next char
                        range->start = cmp + 1;
                    }
                    else
                    {
                        // scan over
                        range->start = end;
                    }

                    ALog_D
                    (
                        "ABufferReader TryFindString found str = %.*s, after str = '%c'",
                        cmp - start + 1,
                        buffer + start,
                        buffer[range->start]
                    );

                    return true;
                }

                cmp++;

                if (cmp > end)
                {
                    // scan over
                    goto NotFoundStr;
                }

                if (buffer[cmp] != str[pos])
                {
                    // compare failure reset to first
                    pos = 0;
                    break;
                }
            }
        }
    }

    NotFoundStr:
    ALog_D("ABufferReader TryFindString not found str = %s", str);
    // keep range->start
    return false;
}


static bool TryFindStringByMemcmp(const char* buffer, ArrayRange* range, const char* str)
{
    CheckRange("TryFindString");

    char* bufferStart = (char*) buffer + range->start;
    int   len1        = range->end     - range->start + 1; // count [start, end]
    int   len2        = (int)   strlen(str);

    while (len1 >= len2)
    {
        if (memcmp(bufferStart, str, len2) == 0)
        {
            // move to next char
            // newStart + 1 = ((range->end - len1 + 1) + len2 - 1) + 1
            range->start = range->end - len1 + len2 + 1;

            if (range->start > range->end)
            {
                range->start = range->end;
            }

            ALog_D
            (
                "ABufferReader TryFindString found str = %.*s, after str = '%c'",
                len2,
                bufferStart,
                buffer[range->start]
            );
            
            return true;
        }

        --len1;
        ++bufferStart;
    }

    ALog_D("ABufferReader TryFindString not found str = %s", str);
    // keep range->start
    return false;
}


struct ABufferReader ABufferReader[1] =
{
    ReadInt64,
    ReadInt32,
    ReadInt16,
    ReadInt8,
    ReadLine,
    TryFindStringByMemcmp,
};


#undef ReadLineLog

