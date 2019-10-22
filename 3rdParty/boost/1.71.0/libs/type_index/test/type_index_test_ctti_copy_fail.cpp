//
// Copyright 2012-2019 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/type_index/ctti_type_index.hpp>

int main() {
    using namespace boost::typeindex;
    ctti_type_index::type_info_t t = ctti_type_index::type_id<int>().type_info();
    (void)t;
}

