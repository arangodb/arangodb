// Copyright (C) 2019 Paul Keir
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/yap.hpp>

#include <boost/core/lightweight_test.hpp>

namespace yap = boost::yap;

int main()
{
    // Test replacements(), which returns a transform object
    {
        using namespace boost::yap::literals;

        auto expr_in  = 1_p * 2_p;

        auto xform    = yap::replacements(6,9);
        auto expr_out = yap::transform(expr_in,xform);
        auto result   = yap::evaluate(expr_out);
        BOOST_TEST(result == 54);

#ifndef _MSC_VER // Tsk, tsk.
        constexpr auto cxform    = yap::replacements(6);
        constexpr auto cexpr_out = yap::transform(1_p,cxform);
        constexpr auto cresult   = yap::evaluate(cexpr_out);
        static_assert(cresult == 6,"");
#endif
    }

    // Test evaluation(), which returns a transform object
    {
        using namespace boost::yap::literals;

        auto expr_in  = 1_p * 2_p;

        auto xform    = yap::evaluation(7,10);
        auto result   = yap::transform(expr_in,xform);
        BOOST_TEST(result == 70);

#ifndef _MSC_VER // Tsk, tsk.
        constexpr auto cxform    = yap::evaluation(7);
        constexpr auto cresult   = yap::transform(1_p,cxform);
        static_assert(cresult == 7,"");
#endif
    }

    // Test replace_placeholders(), which returns an expression 
    {
        using namespace boost::yap::literals;

        auto expr_in  = 1_p * 2_p;

        auto expr_out = yap::replace_placeholders(expr_in,8,11);
        auto result   = yap::evaluate(expr_out);
        BOOST_TEST(result == 88);

#ifndef _MSC_VER // Tsk, tsk.
        constexpr auto cexpr_out = yap::replace_placeholders(1_p,8);
        constexpr auto cresult   = yap::evaluate(cexpr_out);
        static_assert(cresult == 8,"");
#endif
    }

    return boost::report_errors();
}

