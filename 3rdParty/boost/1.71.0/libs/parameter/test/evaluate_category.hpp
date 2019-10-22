// Copyright Cromwell D. Enage 2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef LIBS_PARAMETER_TEST_EVALUTE_CATEGORY_HPP
#define LIBS_PARAMETER_TEST_EVALUTE_CATEGORY_HPP

namespace test {

    enum invoked
    {
        passed_by_lvalue_reference_to_const
      , passed_by_lvalue_reference
      , passed_by_rvalue_reference_to_const
      , passed_by_rvalue_reference
    };

    float rvalue_float()
    {
        return 0.0f;
    }

    float const rvalue_const_float()
    {
        return 0.0f;
    }

    float& lvalue_float()
    {
        static float lfloat = 0.0f;
        return lfloat;
    }

    float const& lvalue_const_float()
    {
        static float const clfloat = 0.0f;
        return clfloat;
    }

    char const* rvalue_char_ptr()
    {
        return "foo";
    }

    char const* const rvalue_const_char_ptr()
    {
        return "foo";
    }

    char const*& lvalue_char_ptr()
    {
        static char const* larr = "foo";
        return larr;
    }

    char const* const& lvalue_const_char_ptr()
    {
        static char const* const clarr = "foo";
        return clarr;
    }
} // namespace test

#include <string>

namespace test {

    std::string rvalue_str()
    {
        return std::string("bar");
    }

    std::string const rvalue_const_str()
    {
        return std::string("bar");
    }

    std::string& lvalue_str()
    {
        static std::string lstr = std::string("bar");
        return lstr;
    }

    std::string const& lvalue_const_str()
    {
        static std::string const clstr = std::string("bar");
        return clstr;
    }
} // namespace test

#include <bitset>

namespace test {

    template <std::size_t N>
    std::bitset<N + 1> rvalue_bitset()
    {
        return std::bitset<N + 1>();
    }

    template <std::size_t N>
    std::bitset<N + 1> const rvalue_const_bitset()
    {
        return std::bitset<N + 1>();
    }

    template <std::size_t N>
    std::bitset<N + 1>& lvalue_bitset()
    {
        static std::bitset<N + 1> lset = std::bitset<N + 1>();
        return lset;
    }

    template <std::size_t N>
    std::bitset<N + 1> const& lvalue_const_bitset()
    {
        static std::bitset<N + 1> const clset = std::bitset<N + 1>();
        return clset;
    }

    template <std::size_t N>
    struct lvalue_bitset_function
    {
        typedef std::bitset<N + 1>& result_type;

        result_type operator()() const
        {
            return test::lvalue_bitset<N>();
        }
    };

    template <std::size_t N>
    struct lvalue_const_bitset_function
    {
        typedef std::bitset<N + 1> const& result_type;

        result_type operator()() const
        {
            return test::lvalue_const_bitset<N>();
        }
    };

    template <std::size_t N>
    struct rvalue_bitset_function
    {
        typedef std::bitset<N + 1> result_type;

        result_type operator()() const
        {
            return test::rvalue_bitset<N>();
        }
    };

    template <std::size_t N>
    struct rvalue_const_bitset_function
    {
        typedef std::bitset<N + 1> const result_type;

        result_type operator()() const
        {
            return test::rvalue_const_bitset<N>();
        }
    };
} // namespace test

#include <boost/parameter/config.hpp>

namespace test {

    template <typename T>
    struct A
    {
        static test::invoked evaluate_category(T const&)
        {
            return test::passed_by_lvalue_reference_to_const;
        }

        static test::invoked evaluate_category(T&)
        {
            return test::passed_by_lvalue_reference;
        }

#if defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
        static test::invoked evaluate_category(T const&&)
        {
            return test::passed_by_rvalue_reference_to_const;
        }

        static test::invoked evaluate_category(T&&)
        {
            return test::passed_by_rvalue_reference;
        }
#endif
    };

    struct U
    {
        template <std::size_t N>
        static test::invoked evaluate_category(std::bitset<N + 1> const&)
        {
            return test::passed_by_lvalue_reference_to_const;
        }

        template <std::size_t N>
        static test::invoked evaluate_category(std::bitset<N + 1>&)
        {
            return test::passed_by_lvalue_reference;
        }

#if defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)
        template <std::size_t N>
        static test::invoked evaluate_category(std::bitset<N + 1> const&&)
        {
            return test::passed_by_rvalue_reference_to_const;
        }

        template <std::size_t N>
        static test::invoked evaluate_category(std::bitset<N + 1>&&)
        {
            return test::passed_by_rvalue_reference;
        }
#endif
    };
} // namespace test

#include <boost/parameter/value_type.hpp>

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <type_traits>
#else
#include <boost/mpl/bool.hpp>
#include <boost/mpl/if.hpp>
#include <boost/type_traits/is_convertible.hpp>
#include <boost/type_traits/remove_const.hpp>
#include <boost/type_traits/remove_pointer.hpp>
#endif

namespace test {

    template <typename CharConstPtrParamTag>
    struct string_predicate
    {
        template <typename Arg, typename Args>
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
        using fn = std::is_convertible<
            Arg
          , std::basic_string<
                typename std::remove_const<
                    typename std::remove_pointer<
                        typename boost::parameter::value_type<
                            Args
                          , CharConstPtrParamTag
#if !defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE)
                          , char const*
#endif
                        >::type
                    >::type
                >::type
            >
        >;
#else   // !defined(BOOST_PARAMETER_CAN_USE_MP11)
        struct apply
          : boost::mpl::if_<
                boost::is_convertible<
                    Arg
                  , std::basic_string<
                        typename boost::remove_const<
                            typename boost::remove_pointer<
                                typename boost::parameter::value_type<
                                    Args
                                  , CharConstPtrParamTag
#if !defined(LIBS_PARAMETER_TEST_COMPILE_FAILURE)
                                  , char const*
#endif
                                >::type
                            >::type
                        >::type
                    >
                >
              , boost::mpl::true_
              , boost::mpl::false_
            >
        {
        };
#endif  // BOOST_PARAMETER_CAN_USE_MP11
    };
} // namespace test

#endif  // include guard

