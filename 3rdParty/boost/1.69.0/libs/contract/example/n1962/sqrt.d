
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[n1962_sqrt_d
// Extra spaces, newlines, etc. for visual alignment with this library code.



long lsqrt(long x)
in {
    assert(x >= 0);
} 
out(result) {
    assert(result * result <= x);
    assert((result + 1) * (result + 1) > x);
} 
do {
    return cast(long)std.math.sqrt(cast(real)x);
}








// End.
//]

