// Copyright 2019, 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/endian/conversion.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/type_traits/enable_if.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/cstdint.hpp>

namespace N
{

struct X
{
    boost::uint32_t m;
};

template<class T> typename boost::enable_if_<boost::is_same<T, X>::value, T>::type endian_reverse( T x )
{
    using boost::endian::endian_reverse;

    X r = { endian_reverse( x.m ) };
    return r;
}

} // namespace N

int main()
{
    using namespace boost::endian;

    N::X x1 = { 0x01020304 };
    N::X x2 = endian_reverse( x1 );

    BOOST_TEST_EQ( x2.m, 0x04030201 );

    return boost::report_errors();
}
