// Copyright Daniel Wallin 2006.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/parameter/config.hpp>
#include <boost/parameter/preprocessor.hpp>
#include <boost/parameter/name.hpp>
#include <boost/tuple/tuple.hpp>
#include <map>
#include <string>
#include "basics.hpp"

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <boost/parameter/value_type.hpp>
#include <boost/mp11/map.hpp>
#include <boost/core/enable_if.hpp>
#include <type_traits>
#else
#include <boost/mpl/bool.hpp>
#include <boost/mpl/if.hpp>
#include <boost/type_traits/is_convertible.hpp>
#if !defined(BOOST_NO_SFINAE)
#include <boost/parameter/value_type.hpp>
#include <boost/mpl/has_key.hpp>
#include <boost/core/enable_if.hpp>
#include <boost/type_traits/is_same.hpp>
#endif
#endif

namespace test {

    BOOST_PARAMETER_NAME(expected)
    BOOST_PARAMETER_NAME(x)
    BOOST_PARAMETER_NAME(y)
    BOOST_PARAMETER_NAME(z)

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
    template <typename To>
    struct predicate
    {
        template <typename From, typename Args>
        using fn = std::is_convertible<From,To>;
    };

    BOOST_PARAMETER_FUNCTION((int), f, test::tag,
        (required
            (expected, *)
        )
        (deduced
            (required
                (x, *(test::predicate<int>))
                (y, *(test::predicate<std::string>))
            )
        )
    )
#else   // !defined(BOOST_PARAMETER_CAN_USE_MP11)
    struct predicate_int
    {
        template <typename From, typename Args>
        struct apply
          : boost::mpl::if_<
                boost::is_convertible<From,int>
              , boost::mpl::true_
              , boost::mpl::false_
            >
        {
        };
    };

    struct predicate_string
    {
        template <typename From, typename Args>
        struct apply
          : boost::mpl::if_<
                boost::is_convertible<From,std::string>
              , boost::mpl::true_
              , boost::mpl::false_
            >
        {
        };
    };

    BOOST_PARAMETER_FUNCTION((int), f, test::tag,
        (required
            (expected, *)
        )
        (deduced
            (required
                (x, *(test::predicate_int))
                (y, *(test::predicate_string))
            )
        )
    )
#endif  // BOOST_PARAMETER_CAN_USE_MP11
    {
        BOOST_TEST(test::equal(x, boost::tuples::get<0>(expected)));
        BOOST_TEST(test::equal(y, boost::tuples::get<1>(expected)));
        return 1;
    }

    struct X 
    {
        X(int x_ = -1) : x(x_)
        {
        }

        bool operator==(X const& other) const
        {
            return this->x == other.x;
        }

        int x;
    };

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
    BOOST_PARAMETER_FUNCTION((int), g, test::tag,
        (required
            (expected, *)
        )
        (deduced
            (required
                (x, *(test::predicate<int>))
                (y, *(test::predicate<std::string>))
            )
            (optional
                (z, *(test::predicate<test::X>), test::X())
            )
        )
    )
#else   // !defined(BOOST_PARAMETER_CAN_USE_MP11)
    struct predicate_X
    {
        template <typename From, typename Args>
        struct apply
          : boost::mpl::if_<
                boost::is_convertible<From,test::X>
              , boost::mpl::true_
              , boost::mpl::false_
            >
        {
        };
    };

    BOOST_PARAMETER_FUNCTION((int), g, tag,
        (required
            (expected, *)
        )
        (deduced
            (required
                (x, *(test::predicate_int))
                (y, *(test::predicate_string))
            )
            (optional
                (z, *(test::predicate_X), test::X())
            )
        )
    )
#endif  // BOOST_PARAMETER_CAN_USE_MP11
    {
        BOOST_TEST(test::equal(x, boost::tuples::get<0>(expected)));
        BOOST_TEST(test::equal(y, boost::tuples::get<1>(expected)));
        BOOST_TEST(test::equal(z, boost::tuples::get<2>(expected)));
        return 1;
    }

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
    BOOST_PARAMETER_FUNCTION((int), sfinae, test::tag,
        (deduced
            (required
                (x, *(test::predicate<std::string>))
            )
        )
    )
#else
    BOOST_PARAMETER_FUNCTION((int), sfinae, test::tag,
        (deduced
            (required
                (x, *(test::predicate_string))
            )
        )
    )
#endif
    {
        return 1;
    }

#if !defined(BOOST_NO_SFINAE)
    // On compilers that actually support SFINAE, add another overload
    // that is an equally good match and can only be in the overload set
    // when the others are not.  This tests that the SFINAE is actually
    // working.  On all other compilers we're just checking that everything
    // about SFINAE-enabled code will work, except of course the SFINAE.
    template <typename A0>
    typename boost::enable_if<
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
        std::is_same<int,A0>
#else
        typename boost::mpl::if_<
            boost::is_same<int,A0>
          , boost::mpl::true_
          , boost::mpl::false_
        >::type
#endif
      , int
    >::type
        sfinae(A0 const& a0)
    {
        return 0;
    }

#if defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE_VENDOR_SPECIFIC) || \
    !BOOST_WORKAROUND(BOOST_MSVC, < 1800)
    // Test support for two different Boost.Parameter-enabled
    // function call operator overloads.
    class char_read_base
    {
        int index;
        char const* key;

     public:
        template <typename Args>
        explicit char_read_base(Args const& args)
          : index(args[test::_y]), key(args[test::_z])
        {
        }

        BOOST_PARAMETER_FUNCTION_CALL_OPERATOR((void), test::tag,
            (deduced
                (required
                    (y, (int))
                    (z, (char const*))
                )
            )
        )
        {
            this->index = y;
            this->key = z;
        }

        BOOST_PARAMETER_CONST_FUNCTION_CALL_OPERATOR((char), test::tag,
            (deduced
                (required
                    (y, (bool))
                    (z, (std::map<char const*,std::string>))
                )
            )
        )
        {
            return y ? (
                (z.find(this->key)->second)[this->index]
            ) : this->key[this->index];
        }
    };

    struct char_reader : public char_read_base
    {
        BOOST_PARAMETER_CONSTRUCTOR(char_reader, (char_read_base), test::tag,
            (deduced
                (required
                    (y, (int))
                    (z, (char const*))
                )
            )
        )
    };
#endif  // MSVC-11.0-

    // Test Boost.Parameter-enabled functions
    // with parameter-dependent return types.
#if !defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE)
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
    BOOST_PARAMETER_FUNCTION(
        (
            boost::lazy_enable_if<
                boost::mp11::mp_map_contains<Args,test::tag::y>
              , boost::parameter::value_type<Args,test::tag::y>
            >
        ), return_y, test::tag,
        (deduced
            (required
                (x, (std::map<char const*,std::string>))
                (y, (char const*))
            )
            (optional
                (z, (int), 4)
            )
        )
    )
#else   // !defined(BOOST_PARAMETER_CAN_USE_MP11)
    BOOST_PARAMETER_FUNCTION(
        (
            boost::lazy_enable_if<
                typename boost::mpl::has_key<Args,test::tag::y>::type
              , boost::parameter::value_type<Args,test::tag::y>
            >
        ), return_y, test::tag,
        (deduced
            (required
                (x, (std::map<char const*,std::string>))
                (y, (char const*))
            )
            (optional
                (z, (int), 4)
            )
        )
    )
#endif  // BOOST_PARAMETER_CAN_USE_MP11
    {
        return y;
    }
#endif  // LIBS_PARAMETER_TEST_COMPILE_FAILURE
#endif  // BOOST_NO_SFINAE

#if defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE)
    BOOST_PARAMETER_FUNCTION(
        (typename boost::parameter::value_type<Args,test::tag::y>::type),
        return_y, test::tag,
        (deduced
            (required
                (x, (std::map<char const*,std::string>))
                (y, (char const*))
            )
            (optional
                (z, (int), 4)
            )
        )
    )
    {
        return y;
    }
#endif  // defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE)
} // namespace test

#include <boost/core/lightweight_test.hpp>

int main()
{
    test::f(
        boost::make_tuple(0, std::string("foo"))
      , test::_x = 0
      , test::_y = std::string("foo")
    );
    test::f(
        boost::make_tuple(0, std::string("foo"))
      , 0
      , std::string("foo")
    );
    test::f(
        boost::make_tuple(0, std::string("foo"))
      , std::string("foo")
      , 0
    );
    test::f(
        boost::make_tuple(0, std::string("foo"))
      , test::_y = std::string("foo")
      , 0
    );
    test::f(
        boost::make_tuple(0, std::string("foo"))
      , test::_x = 0
      , std::string("foo")
    );
    test::f(
        boost::make_tuple(0, std::string("foo"))
      , 0
      , test::_y = std::string("foo")
    );
    test::g(
        boost::make_tuple(0, std::string("foo"), test::X())
      , test::_x = 0
      , test::_y = std::string("foo")
    );
    test::g(
        boost::make_tuple(0, std::string("foo"), test::X())
      , 0
      , std::string("foo")
    );
    test::g(
        boost::make_tuple(0, std::string("foo"), test::X())
      , std::string("foo")
      , 0
    );
    test::g(
        boost::make_tuple(0, std::string("foo"), test::X())
      , test::_y = std::string("foo")
      , 0
    );
    test::g(
        boost::make_tuple(0, std::string("foo"), test::X())
      , test::_x = 0
      , std::string("foo")
    );
    test::g(
        boost::make_tuple(0, std::string("foo"), test::X())
      , 0
      , test::_y = std::string("foo")
    );
    test::g(
        boost::make_tuple(0, std::string("foo"), test::X(1))
      , 0
      , test::_y = std::string("foo")
      , test::X(1)
    );
    test::g(
        boost::make_tuple(0, std::string("foo"), test::X(1))
      , test::X(1)
      , 0
      , test::_y = std::string("foo")
    );

    std::map<char const*,std::string> k2s;
#if !defined(BOOST_NO_SFINAE)
    char const* keys[] = {"foo", "bar", "baz"};
    BOOST_TEST_EQ(1, test::sfinae(keys[0]));
    BOOST_TEST_EQ(0, test::sfinae(0));
#if defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE_VENDOR_SPECIFIC) || \
    !BOOST_WORKAROUND(BOOST_MSVC, < 1800)
    k2s[keys[0]] = std::string("qux");
    k2s[keys[1]] = std::string("wmb");
    k2s[keys[2]] = std::string("zxc");
    test::char_reader r(keys[0], 0);
    BOOST_TEST_EQ('q', (r(k2s, true)));
    BOOST_TEST_EQ('f', (r(k2s, false)));
    r(keys[1], 1);
    BOOST_TEST_EQ('m', (r(k2s, true)));
    BOOST_TEST_EQ('a', (r(k2s, false)));
    r(keys[2], 2);
    BOOST_TEST_EQ('c', (r(k2s, true)));
    BOOST_TEST_EQ('z', (r(k2s, false)));
#endif  // MSVC-11.0-
#if !defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE)
    BOOST_TEST_EQ(keys[1], test::return_y(2, k2s, keys[1]));
#endif
#endif  // BOOST_NO_SFINAE

#if defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE)
    BOOST_TEST_EQ(keys[1], test::return_y(2, k2s, keys[1]));
#endif

    return boost::report_errors();
}

