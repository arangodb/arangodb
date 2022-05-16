// Copyright (C) 2019 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include "static_vector.hpp"


int main()
{
    //[ static_vector_usage
    static_vector<int, 128> vec;
    vec.push_back(42);
    vec.push_back(13);
    assert(vec[0] == 42);
    assert(vec[1] == 13);
    assert(vec.size() == 2u);

    vec.erase(vec.end() - 1);
    static_vector<int, 128> tmp({42});
    assert(vec == (static_vector<int, 128>({42})));
    //]
}
