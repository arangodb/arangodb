//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
// Copyright 2005-2007 Adobe Systems Incorporated
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/channel.hpp>
#include <boost/gil/color_base_algorithm.hpp>
#include <boost/gil/concepts/pixel.hpp>
#include <boost/gil/pixel.hpp>
#include <boost/gil/planar_pixel_reference.hpp>
#include <boost/gil/promote_integral.hpp>
#include <boost/gil/typedefs.hpp>

#include <boost/core/typeinfo.hpp>
#include <boost/mp11.hpp>
#include <boost/mp11/mpl.hpp> // for compatibility with Boost.Test

#include <cstdint>
#include <tuple>
#include <type_traits>

namespace boost { namespace gil {

namespace detail {

struct print_color_base
{
    std::ostream& os_;
    std::size_t element_index_{0};
    print_color_base(std::ostream& os) : os_(os) {}

    template <typename Element>
    void operator()(Element& c)
    {
        typename gil::promote_integral<Element>::type const v(c);
        if (element_index_ > 0) os_ << ", ";
        os_ << "v" << element_index_ << "=" << v;
        ++element_index_;
    }
};

} // namespace detail

// Pixel has to implement operator<< to be printable for BOOST_TEST()
template <typename ChannelValue, typename Layout>
std::ostream& operator<<(std::ostream& os, pixel<ChannelValue, Layout> const& p)
{
    os << "pixel<"
       << "Channel=" << boost::core::demangled_name(typeid(ChannelValue))
       << ", Layout=" << boost::core::demangled_name(typeid(Layout))
       << ">(";

    gil::static_for_each(p, detail::print_color_base{os});
    os << ")" << std::endl;
    return os;
}

namespace test { namespace fixture {

template <typename Pixel, int Tag = 0>
class pixel_value
{
public:
    using type = Pixel;
    using pixel_t = type;
    type pixel_{};

    pixel_value() = default;
    explicit pixel_value(pixel_t const& pixel)
        : pixel_(pixel) // test copy constructor
    {
        type temp_pixel; // test default constructor
        boost::function_requires<PixelValueConcept<pixel_t> >();
    }
};

// Alias compatible with naming of equivalent class in the test/legacy/pixel.cpp
// The core suffix indicates `Pixel` is GIL core pixel type.
template <typename Pixel, int Tag = 0>
using value_core = pixel_value<Pixel, Tag>;

template <typename PixelRef, int Tag = 0>
struct pixel_reference
    : pixel_value
    <
        typename std::remove_reference<PixelRef>::type,
        Tag
    >
{
    static_assert(
        std::is_reference<PixelRef>::value ||
        gil::is_planar<PixelRef>::value, // poor-man test for specialization of planar_pixel_reference
        "PixelRef must be reference or gil::planar_pixel_reference");

    using type = PixelRef;
    using pixel_t = typename std::remove_reference<PixelRef>::type;
    using parent_t = pixel_value<typename pixel_t::value_type, Tag>;
    using value_t = typename pixel_t::value_type;
    type pixel_{}; // reference

    pixel_reference() : parent_t{}, pixel_(parent_t::pixel_) {}
    explicit pixel_reference(value_t const& pixel) : parent_t(pixel), pixel_(parent_t::pixel_)
    {
        boost::function_requires<PixelConcept<pixel_t>>();
    }
};

// Alias compatible with naming of equivalent class in the test/legacy/pixel.cpp
// The core suffix indicates `Pixel` is GIL core pixel type.
template <typename Pixel, int Tag = 0>
using reference_core = pixel_reference<Pixel, Tag>;

// Metafunction to yield nested type of a representative pixel type
template <typename PixelValueOrReference>
using nested_type = typename PixelValueOrReference::type;

// Metafunction to yield nested type of a representative pixel_t type
template <typename PixelValueOrReference>
using nested_pixel_type = typename PixelValueOrReference::pixel_t;

// Subset of pixel models that covers all color spaces, channel depths,
// reference/value, planar/interleaved, const/mutable.
// Operations like color conversion will be invoked on pairs of those.
using representative_pixel_types= ::boost::mp11::mp_list
<
    value_core<gil::gray8_pixel_t>,
    reference_core<gil::gray16_pixel_t&>,
    value_core<gil::bgr8_pixel_t>,
    reference_core<gil::rgb8_planar_ref_t>,
    value_core<gil::argb32_pixel_t>,
    reference_core<gil::cmyk32f_pixel_t&>,
    reference_core<gil::abgr16c_ref_t>, // immutable reference
    reference_core<gil::rgb32fc_planar_ref_t>
>;

// List of all core pixel types (i.e. without cv-qualifiers)
using pixel_types = ::boost::mp11::mp_list
<
    gil::gray8_pixel_t,
    gil::gray8s_pixel_t,
    gil::gray16_pixel_t,
    gil::gray16s_pixel_t,
    gil::gray32_pixel_t,
    gil::gray32s_pixel_t,
    gil::gray32f_pixel_t,
    gil::bgr8_pixel_t,
    gil::bgr8s_pixel_t,
    gil::bgr16_pixel_t,
    gil::bgr16s_pixel_t,
    gil::bgr32_pixel_t,
    gil::bgr32f_pixel_t,
    gil::bgr32s_pixel_t,
    gil::rgb8_pixel_t,
    gil::rgb8s_pixel_t,
    gil::rgb16_pixel_t,
    gil::rgb16s_pixel_t,
    gil::rgb32_pixel_t,
    gil::rgb32f_pixel_t,
    gil::rgb32s_pixel_t,
    gil::abgr8_pixel_t,
    gil::abgr8s_pixel_t,
    gil::abgr16_pixel_t,
    gil::abgr16s_pixel_t,
    gil::abgr32_pixel_t,
    gil::abgr32f_pixel_t,
    gil::abgr32s_pixel_t,
    gil::bgra8_pixel_t,
    gil::bgra8s_pixel_t,
    gil::bgra16_pixel_t,
    gil::bgra16s_pixel_t,
    gil::bgra32_pixel_t,
    gil::bgra32f_pixel_t,
    gil::bgra32s_pixel_t,
    gil::cmyk8_pixel_t,
    gil::cmyk8s_pixel_t,
    gil::cmyk16_pixel_t,
    gil::cmyk16s_pixel_t,
    gil::cmyk32_pixel_t,
    gil::cmyk32f_pixel_t,
    gil::cmyk32s_pixel_t,
    gil::rgba8_pixel_t,
    gil::rgba8s_pixel_t,
    gil::rgba16_pixel_t,
    gil::rgba16s_pixel_t,
    gil::rgba32_pixel_t,
    gil::rgba32f_pixel_t,
    gil::rgba32s_pixel_t
>;

// List of all core pixel typedefs (i.e. with cv-qualifiers)
using pixel_typedefs = ::boost::mp11::mp_list
<
    gil::gray8_pixel_t,
    gil::gray8c_pixel_t,
    gil::gray8s_pixel_t,
    gil::gray8sc_pixel_t,
    gil::gray16_pixel_t,
    gil::gray16c_pixel_t,
    gil::gray16s_pixel_t,
    gil::gray16sc_pixel_t,
    gil::gray32_pixel_t,
    gil::gray32c_pixel_t,
    gil::gray32f_pixel_t,
    gil::gray32fc_pixel_t,
    gil::gray32s_pixel_t,
    gil::gray32sc_pixel_t,
    gil::bgr8_pixel_t,
    gil::bgr8c_pixel_t,
    gil::bgr8s_pixel_t,
    gil::bgr8sc_pixel_t,
    gil::bgr16_pixel_t,
    gil::bgr16c_pixel_t,
    gil::bgr16s_pixel_t,
    gil::bgr16sc_pixel_t,
    gil::bgr32_pixel_t,
    gil::bgr32c_pixel_t,
    gil::bgr32f_pixel_t,
    gil::bgr32fc_pixel_t,
    gil::bgr32s_pixel_t,
    gil::bgr32sc_pixel_t,
    gil::rgb8_pixel_t,
    gil::rgb8c_pixel_t,
    gil::rgb8s_pixel_t,
    gil::rgb8sc_pixel_t,
    gil::rgb16_pixel_t,
    gil::rgb16c_pixel_t,
    gil::rgb16s_pixel_t,
    gil::rgb16sc_pixel_t,
    gil::rgb32_pixel_t,
    gil::rgb32c_pixel_t,
    gil::rgb32f_pixel_t,
    gil::rgb32fc_pixel_t,
    gil::rgb32s_pixel_t,
    gil::rgb32sc_pixel_t,
    gil::abgr8_pixel_t,
    gil::abgr8c_pixel_t,
    gil::abgr8s_pixel_t,
    gil::abgr8sc_pixel_t,
    gil::abgr16_pixel_t,
    gil::abgr16c_pixel_t,
    gil::abgr16s_pixel_t,
    gil::abgr16sc_pixel_t,
    gil::abgr32_pixel_t,
    gil::abgr32c_pixel_t,
    gil::abgr32f_pixel_t,
    gil::abgr32fc_pixel_t,
    gil::abgr32s_pixel_t,
    gil::abgr32sc_pixel_t,
    gil::bgra8_pixel_t,
    gil::bgra8c_pixel_t,
    gil::bgra8s_pixel_t,
    gil::bgra8sc_pixel_t,
    gil::bgra16_pixel_t,
    gil::bgra16c_pixel_t,
    gil::bgra16s_pixel_t,
    gil::bgra16sc_pixel_t,
    gil::bgra32_pixel_t,
    gil::bgra32c_pixel_t,
    gil::bgra32f_pixel_t,
    gil::bgra32fc_pixel_t,
    gil::bgra32s_pixel_t,
    gil::bgra32sc_pixel_t,
    gil::cmyk8_pixel_t,
    gil::cmyk8c_pixel_t,
    gil::cmyk8s_pixel_t,
    gil::cmyk8sc_pixel_t,
    gil::cmyk16_pixel_t,
    gil::cmyk16c_pixel_t,
    gil::cmyk16s_pixel_t,
    gil::cmyk16sc_pixel_t,
    gil::cmyk32_pixel_t,
    gil::cmyk32c_pixel_t,
    gil::cmyk32f_pixel_t,
    gil::cmyk32fc_pixel_t,
    gil::cmyk32s_pixel_t,
    gil::cmyk32sc_pixel_t,
    gil::rgba8_pixel_t,
    gil::rgba8c_pixel_t,
    gil::rgba8s_pixel_t,
    gil::rgba8sc_pixel_t,
    gil::rgba16_pixel_t,
    gil::rgba16c_pixel_t,
    gil::rgba16s_pixel_t,
    gil::rgba16sc_pixel_t,
    gil::rgba32_pixel_t,
    gil::rgba32c_pixel_t,
    gil::rgba32f_pixel_t,
    gil::rgba32fc_pixel_t,
    gil::rgba32s_pixel_t,
    gil::rgba32sc_pixel_t
>;

struct not_a_pixel_type {};

using non_pixels = ::boost::mp11::mp_list
<
    not_a_pixel_type,
    char,
    short, int, long,
    double, float,
    std::size_t,
    std::true_type,
    std::false_type
>;


}}}} // namespace boost::gil::test::fixture
