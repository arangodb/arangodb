//
// Copyright 2019-2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distribtted under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_TEST_TEST_UTILITY_HPP
#define BOOST_GIL_TEST_TEST_UTILITY_HPP

#include <boost/gil/color_base_algorithm.hpp> // static_for_each
#include <boost/gil/packed_pixel.hpp>
#include <boost/gil/pixel.hpp>
#include <boost/gil/planar_pixel_reference.hpp>
#include <boost/gil/promote_integral.hpp>

#include <boost/core/demangle.hpp>
#include <boost/core/typeinfo.hpp>

#include <cstdint>
#include <ostream>
#include <type_traits>

// Utilities to make GIL primitives printable for BOOST_TEST_EQ and other macros

namespace boost { namespace gil {

namespace test { namespace utility {

template <typename T>
struct printable_numeric
{
    using type = typename std::conditional
    <
        std::is_integral<T>::value,
        typename ::boost::gil::promote_integral<T>::type,
        typename std::common_type<T, double>::type
    >::type;

    static_assert(std::is_arithmetic<T>::value, "T must be numeric type");
    static_assert(sizeof(T) <= sizeof(type), "bit-size narrowing conversion");
};

template <typename T>
using printable_numeric_t = typename printable_numeric<T>::type;

struct print_color_base
{
    std::ostream& os_;
    std::size_t element_index_{0};
    print_color_base(std::ostream& os) : os_(os) {}

    template <typename Element>
    void operator()(Element const& c)
    {
        printable_numeric_t<Element> n{c};
        if (element_index_ > 0) os_ << ", ";
        os_ << "v" << element_index_ << "=" << n;
        ++element_index_;
    }

    template <typename BitField, int FirstBit, int NumBits, bool IsMutable>
    void operator()(gil::packed_channel_reference<BitField, FirstBit, NumBits, IsMutable> const& c)
    {
        printable_numeric_t<BitField> n{c.get()};
        if (element_index_ > 0) os_ << ", ";
        os_ << "v" << element_index_ << "=" << n;
        ++element_index_;
    }

    template <typename BaseChannelValue, typename MinVal, typename MaxVal>
    void operator()(gil::scoped_channel_value<BaseChannelValue, MinVal, MaxVal> const& c)
    {
        printable_numeric_t<BaseChannelValue> n{c};
        if (element_index_ > 0) os_ << ", ";
        os_ << "v" << element_index_ << "=" << n;
        ++element_index_;
    }
};

}} // namespace test::utility

template <typename T>
std::ostream& operator<<(std::ostream& os, point<T> const& p)
{
    os << "point<" << boost::core::demangled_name(typeid(T)) << ">";
    os << "(" << p.x << ", " << p.y << ")" << std::endl;
    return os;
}

template <typename ChannelValue, typename Layout>
std::ostream& operator<<(std::ostream& os, pixel<ChannelValue, Layout> const& p)
{
    os << "pixel<"
       << "\n\tChannel=" << boost::core::demangled_name(typeid(ChannelValue))
       << ",\n\tLayout=" << boost::core::demangled_name(typeid(Layout))
       << "\n>(";

    static_for_each(p, test::utility::print_color_base{os});
    os << ")" << std::endl;
    return os;
}

template <typename BitField, typename ChannelRefs, typename Layout>
std::ostream& operator<<(std::ostream& os, packed_pixel<BitField, ChannelRefs, Layout> const& p)
{
    os << "packed_pixel<"
       << "\n\tBitField=" << boost::core::demangled_name(typeid(BitField))
       << ",\n\tChannelRefs=" << boost::core::demangled_name(typeid(ChannelRefs))
       << ",\n\tLayout=" << boost::core::demangled_name(typeid(Layout))
       << ">(";

    static_for_each(p, test::utility::print_color_base{os});
    os << ")" << std::endl;
    return os;
}

template <typename ChannelReference, typename ColorSpace>
std::ostream& operator<<(std::ostream& os, planar_pixel_reference<ChannelReference, ColorSpace> const& p)
{
    os << "planar_pixel_reference<"
       << "\nChannelReference=" << boost::core::demangled_name(typeid(ChannelReference))
       << ",\nColorSpace=" << boost::core::demangled_name(typeid(ColorSpace))
       << ">(";

    static_for_each(p, test::utility::print_color_base{os});
    os << ")" << std::endl;
    return os;
}

}} // namespace boost::gil

#endif
