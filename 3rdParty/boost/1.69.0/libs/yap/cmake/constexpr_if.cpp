// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
int main ()
{
#if CHECK_CONSTEXPR_IF
    if constexpr (true) {
        return 0;
    }
#endif
}
