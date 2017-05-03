
//          Copyright Oliver Kowalke 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>
#include <iostream>

#include <boost/coroutine2/all.hpp>

using namespace boost::coroutines2;

asymmetric_coroutine<int>::pull_type make_dummy_range()
{
    return asymmetric_coroutine<int>::pull_type([](asymmetric_coroutine<int>::push_type& yield)
    {
        yield(1);
    });
}

int main() {
    std::distance(make_dummy_range()); // error
    std::cout << "Done" << std::endl;
    return EXIT_SUCCESS;
}
