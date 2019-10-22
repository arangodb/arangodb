/*=============================================================================
    Copyright (c) 2019 Nikita Kniazev
    Copyright (c) 2019 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include "grammar.hpp"
#include <boost/detail/lightweight_test.hpp>

int main()
{
    char const* s = "123", * e = s + std::strlen(s);
#if 1
    auto r = parse(s, e, grammar);
#else
    int i = 0;
    auto r = parse(s, e, grammar, i);
#endif

    BOOST_TEST(r);
    return boost::report_errors();
}

