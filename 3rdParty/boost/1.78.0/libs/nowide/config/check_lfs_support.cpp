//
//  Copyright (c) 2020 Alexander Grund
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE or copy at http://www.boost.org/LICENSE.txt)
//

#define _LARGEFILE_SOURCE
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include <stdio.h>

void check(FILE* f)
{
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MINGW32__)
    (void)_fseeki64(f, 0, SEEK_CUR);
    (void)_ftelli64(f);
#else
    // Check that those functions and off_t are available
    (void)fseeko(f, off_t(0), SEEK_CUR);
    (void)ftello(f);
#endif
}

int main()
{
    check(nullptr);
}
