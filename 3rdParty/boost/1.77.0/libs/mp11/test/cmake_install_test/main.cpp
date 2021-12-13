// Copyright 2018 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

#include <boost/mp11.hpp>
using namespace boost::mp11;

int main()
{
    using L1 = mp_list<int, float, int, float>;
    return mp_size<mp_unique<L1>>::value == 2? 0: 1;
}
