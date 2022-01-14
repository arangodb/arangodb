//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// FIXME: Avoid Clang's flooding of non-disableable warnings like:
// "T does not declare any constructor to initialize its non-modifiable members"
// when compiling with concepts check enabled.
// See https://bugs.llvm.org/show_bug.cgi?id=41759
#if !defined(BOOST_GIL_USE_CONCEPT_CHECK) && !defined(__clang__)
#error Compile with BOOST_GIL_USE_CONCEPT_CHECK defined
#endif
#include <boost/gil.hpp>

#include <boost/concept_check.hpp>
#include <boost/mp11.hpp>

#include "test_fixture.hpp"

namespace gil = boost::gil;
namespace mp11 = boost::mp11;
using boost::function_requires;

template <template<typename> class Concept>
struct assert_concept
{
    template <typename Pixel>
    void operator()(Pixel&&)
    {
        function_requires<Concept<Pixel>>();
    }
};

template <template<typename> class Concept, typename... Pixels>
void test_concept()
{
    mp11::mp_for_each<Pixels...>(assert_concept<Concept>());
}

int main()
{
    test_concept<gil::PixelConcept, gil::test::fixture::pixel_typedefs>();
    test_concept<gil::MutablePixelConcept, gil::test::fixture::pixel_typedefs>();

    using rgb565_pixel_t = gil::packed_pixel_type
        <
            std::uint16_t,
            mp11::mp_list_c<unsigned, 5, 6, 5>,
            gil::rgb_layout_t
        >::type;
    function_requires<gil::PixelConcept<rgb565_pixel_t>>();
    function_requires<gil::PixelValueConcept<rgb565_pixel_t>>();

    using bgr556_pixel_t = gil::packed_pixel_type
        <
            std::uint16_t,
            mp11::mp_list_c<unsigned, 5, 6, 5>,
            gil::bgr_layout_t>::type;
    function_requires<gil::PixelConcept<bgr556_pixel_t>>();
    function_requires<gil::PixelValueConcept<bgr556_pixel_t>>();

    function_requires<gil::PixelsCompatibleConcept<rgb565_pixel_t, bgr556_pixel_t>>();

    return 0;
}
