/* Copyright (c) Citrix Systems Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, 
 * with or without modification, are permitted provided 
 * that the following conditions are met:
 * 
 * *   Redistributions of source code must retain the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer.
 * *   Redistributions in binary form must reproduce the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer in the documentation and/or other 
 *     materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 */

#ifndef DEBUG_H__
#define DEBUG_H__

#if DBG

#include <stdarg.h>         // va_list
#include <stdio.h>          // vsprintf
#include <malloc.h>

#include <assert.h>
#include <tchar.h>

__inline void DebugPrint( IN LPCTSTR msg, IN ... )
{
    TCHAR   buffer[256];
    int     res;
    va_list args;

    va_start( args, msg );
    res = _vsntprintf_s(buffer, sizeof(buffer) / sizeof(buffer[0]), _TRUNCATE, msg, args);
    if (res >= 0)
    {
        OutputDebugString( buffer );
    }
    else
    {
        TCHAR *p;
        int count;

        count = 512;
        for (;;) {
            p = (TCHAR *)malloc(count * sizeof (TCHAR));
            if (!p) {
                OutputDebugString(_T("Out of memory for debug message!"));
                break;
            }
            res = _vsntprintf_s(p, count, _TRUNCATE, msg, args);
            if (res >= 0)
                break;

            free(p);
            count += 256;
        }
        if (p) {
            OutputDebugString( p );
            free(p);
        }
    }
    va_end(args);
}

#define DBGPRINT(_x_) DebugPrint _x_
#define ASSERT  assert

#else
__inline void DebugPrint( IN LPCTSTR msg, IN ... ) {}
#define DBGPRINT(_x_) 
#define ASSERT  
#endif // DBG

#endif /* !DEBUG_H__ */
