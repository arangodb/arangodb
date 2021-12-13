//
// Copyright 2020 Samuel Debionne
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/dynamic_image/any_image.hpp>

#include <boost/core/lightweight_test.hpp>

#include "test_fixture.hpp"
#include "core/image/test_fixture.hpp"

namespace gil = boost::gil;
namespace fixture = boost::gil::test::fixture;

struct test_any_image_move_ctor
{
    template <typename Image>
    void operator()(Image const&)
    {
        using image_t = Image;
        fixture::dynamic_image i0(fixture::create_image<image_t>(4, 4, 128));
        BOOST_TEST_EQ(i0.dimensions().x, 4);
        BOOST_TEST_EQ(i0.dimensions().y, 4);
        
        fixture::dynamic_image i1 = fixture::create_image<image_t>(4, 4, 128);
        BOOST_TEST_EQ(i1.dimensions().x, 4);
        BOOST_TEST_EQ(i1.dimensions().y, 4);
    }
    static void run()
    {
        boost::mp11::mp_for_each<fixture::image_types>(test_any_image_move_ctor{});
    }
};


int main()
{
    test_any_image_move_ctor::run();

    return ::boost::report_errors();
}
