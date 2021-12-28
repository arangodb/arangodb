//---------------------------------------------------------------------------//
// Copyright (c) 2017 Jakub Szuppe <j.szuppe@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

__kernel void foobar(__global int* x)
{
    const int gid = get_global_id(0);
    x[gid] = gid;
}
