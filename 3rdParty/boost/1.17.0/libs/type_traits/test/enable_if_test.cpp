/*
Copyright 2018 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License,
Version 1.0. (See accompanying file LICENSE_1_0.txt
or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/type_traits/enable_if.hpp>
#include "test.hpp"

template<bool B>
struct Constant {
    enum {
        value = B
    };
};

template<class T>
struct Check
    : Constant<false> { };

template<>
struct Check<long>
    : Constant<true> { };

class Construct {
public:
    template<class T>
    Construct(T, typename boost::enable_if_<Check<T>::value>::type* = 0)
        : value_(true) { }
    template<class T>
    Construct(T, typename boost::enable_if_<!Check<T>::value>::type* = 0)
        : value_(false) { }
    bool value() const {
        return value_;
    }
private:
    bool value_;
};

template<class T, class E = void>
struct Specialize;

template<class T>
struct Specialize<T, typename boost::enable_if_<Check<T>::value>::type>
    : Constant<true> { };

template<class T>
struct Specialize<T, typename boost::enable_if_<!Check<T>::value>::type>
    : Constant<false> { };

template<class T>
typename boost::enable_if_<Check<T>::value, bool>::type Returns(T)
{
    return true;
}

template<class T>
typename boost::enable_if_<!Check<T>::value, bool>::type Returns(T)
{
    return false;
}

#if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
template<class T>
boost::enable_if_t<Check<T>::value, bool> Alias(T)
{
    return true;
}

template<class T>
boost::enable_if_t<!Check<T>::value, bool> Alias(T)
{
    return false;
}
#endif

TT_TEST_BEGIN(enable_if)

BOOST_CHECK(!Construct(1).value());
BOOST_CHECK(Construct(1L).value());
BOOST_CHECK(!Specialize<int>::value);
BOOST_CHECK(Specialize<long>::value);
BOOST_CHECK(!Returns(1));
BOOST_CHECK(Returns(1L));
#if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
BOOST_CHECK(!Alias(1));
BOOST_CHECK(Alias(1L));
#endif

TT_TEST_END
