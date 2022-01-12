// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_LEAF_TEST_SINGLE_HEADER
#   include "leaf.hpp"
#else
#   include <boost/leaf/detail/config.hpp>
#   include <boost/leaf/handle_errors.hpp>
#   include <boost/leaf/result.hpp>
#   include <boost/leaf/common.hpp>
#endif

#include "lightweight_test.hpp"
#include <sstream>

namespace leaf = boost::leaf;

enum class enum_class_payload
{
    value
};

template <int A>
struct unexpected_test
{
    int value;
};

struct my_exception:
    virtual std::exception
{
    char const * what() const noexcept
    {
        return "my_exception";
    }
};

struct printable_payload
{
    friend std::ostream & operator<<( std::ostream & os, printable_payload const & x )
    {
        return os << "printed printable_payload";
    }
};

struct non_printable_payload
{
};

struct printable_info_printable_payload
{
    printable_payload value;

    friend std::ostream & operator<<( std::ostream & os, printable_info_printable_payload const & x )
    {
        return os << "*** printable_info_printable_payload " << x.value << " ***";
    }
};

struct printable_info_non_printable_payload
{
    non_printable_payload value;

    friend std::ostream & operator<<( std::ostream & os, printable_info_non_printable_payload const & x )
    {
        return os << "*** printable_info_non_printable_payload ***";
    }
};

struct non_printable_info_printable_payload
{
    printable_payload value;
};

struct non_printable_info_non_printable_payload
{
    non_printable_payload value;
};

int main()
{
    leaf::try_handle_all(
        []() -> leaf::result<void>
        {
            return BOOST_LEAF_NEW_ERROR(
                printable_info_printable_payload(),
                printable_info_non_printable_payload(),
                non_printable_info_printable_payload(),
                non_printable_info_non_printable_payload(),
                unexpected_test<1>{1},
                unexpected_test<2>{2},
                enum_class_payload{},
                leaf::e_errno{ENOENT} );
        },
        [](
            leaf::e_source_location,
            printable_info_printable_payload,
            printable_info_non_printable_payload,
            non_printable_info_printable_payload,
            non_printable_info_non_printable_payload,
            enum_class_payload,
            leaf::e_errno,
            leaf::error_info const & unmatched )
        {
            std::ostringstream st;
            st << unmatched;
            std::string s = st.str();
            BOOST_TEST_NE(s.find("leaf::error_info: Error ID = "), s.npos);
            std::cout << s;
        },
        []()
        {
            BOOST_ERROR("Bad error dispatch");
        } );

    std::cout << __LINE__  << " ----\n";

    leaf::try_handle_all(
        []() -> leaf::result<void>
        {
            return BOOST_LEAF_NEW_ERROR(
                printable_info_printable_payload(),
                printable_info_non_printable_payload(),
                non_printable_info_printable_payload(),
                non_printable_info_non_printable_payload(),
                unexpected_test<1>{1},
                unexpected_test<2>{2},
                enum_class_payload{},
                leaf::e_errno{ENOENT} );
        },
        [](
            leaf::e_source_location,
            printable_info_printable_payload,
            printable_info_non_printable_payload,
            non_printable_info_printable_payload,
            non_printable_info_non_printable_payload,
            enum_class_payload,
            leaf::e_errno,
            leaf::diagnostic_info const & unmatched )
        {
            std::ostringstream st;
            st << unmatched;
            std::string s = st.str();
#if BOOST_LEAF_DIAGNOSTICS
            BOOST_TEST_NE(s.find("leaf::diagnostic_info for Error ID = "), s.npos);
            BOOST_TEST_NE(s.find("e_source_location"), s.npos);
            BOOST_TEST_NE(s.find("*** printable_info_printable_payload printed printable_payload ***"), s.npos);
            BOOST_TEST_NE(s.find("*** printable_info_non_printable_payload ***"), s.npos);
            BOOST_TEST_NE(s.find(": printed printable_payload"), s.npos);
            BOOST_TEST_NE(s.find(": {Non-Printable}"), s.npos);
            BOOST_TEST_NE(s.find("enum_class_payload"), s.npos);
            BOOST_TEST_NE(s.find("Detected 2 attempts"), s.npos);
            BOOST_TEST_NE(s.find("unexpected_test<1>"), s.npos);
            BOOST_TEST_EQ(s.find("unexpected_test<2>"), s.npos);
#else
            BOOST_TEST_NE(s.find("leaf::diagnostic_info requires #define BOOST_LEAF_DIAGNOSTICS 1"), s.npos);
            BOOST_TEST_NE(s.find("leaf::error_info: Error ID = "), s.npos);
#endif
            std::cout << s;
        },
        []()
        {
            BOOST_ERROR("Bad error dispatch");
        } );

    std::cout << __LINE__  << " ----\n";

    leaf::try_handle_all(
        []() -> leaf::result<void>
        {
            return BOOST_LEAF_NEW_ERROR(
                printable_info_printable_payload(),
                printable_info_non_printable_payload(),
                non_printable_info_printable_payload(),
                non_printable_info_non_printable_payload(),
                unexpected_test<1>{1},
                unexpected_test<2>{2},
                enum_class_payload{},
                leaf::e_errno{ENOENT} );
        },
        [](
            leaf::e_source_location,
            printable_info_printable_payload,
            printable_info_non_printable_payload,
            non_printable_info_printable_payload,
            non_printable_info_non_printable_payload,
            enum_class_payload,
            leaf::e_errno,
            leaf::verbose_diagnostic_info const & di )
        {
            std::ostringstream st;
            st << di;
            std::string s = st.str();
#if BOOST_LEAF_DIAGNOSTICS
            BOOST_TEST_NE(s.find("leaf::verbose_diagnostic_info for Error ID = "), s.npos);
            BOOST_TEST_NE(s.find("e_source_location"), s.npos);
            BOOST_TEST_NE(s.find("*** printable_info_printable_payload printed printable_payload ***"), s.npos);
            BOOST_TEST_NE(s.find("*** printable_info_non_printable_payload ***"), s.npos);
            BOOST_TEST_NE(s.find(": printed printable_payload"), s.npos);
            BOOST_TEST_NE(s.find(": {Non-Printable}"), s.npos);
            BOOST_TEST_NE(s.find("enum_class"), s.npos);
            BOOST_TEST_NE(s.find("Unhandled error objects:"), s.npos);
            BOOST_TEST_NE(s.find("unexpected_test<1>"), s.npos);
            BOOST_TEST_NE(s.find("unexpected_test<2>"), s.npos);
            BOOST_TEST_NE(s.find(": 1"), s.npos);
            BOOST_TEST_NE(s.find(": 2"), s.npos);
#else
            BOOST_TEST_NE(s.find("leaf::verbose_diagnostic_info requires #define BOOST_LEAF_DIAGNOSTICS 1"), s.npos);
            BOOST_TEST_NE(s.find("leaf::error_info: Error ID = "), s.npos);
#endif
            std::cout << s;
        },
        []()
        {
            BOOST_ERROR("Bad error dispatch");
        } );

    std::cout << __LINE__  << " ----\n";

    ///////////////////////////////////

#ifndef BOOST_LEAF_NO_EXCEPTIONS

    leaf::try_catch(
        []
        {
            BOOST_LEAF_THROW_EXCEPTION( my_exception(),
                printable_info_printable_payload(),
                printable_info_non_printable_payload(),
                non_printable_info_printable_payload(),
                non_printable_info_non_printable_payload(),
                unexpected_test<1>{1},
                unexpected_test<2>{2},
                enum_class_payload{},
                leaf::e_errno{ENOENT} );
        },
        [](
            leaf::e_source_location,
            printable_info_printable_payload,
            printable_info_non_printable_payload,
            non_printable_info_printable_payload,
            non_printable_info_non_printable_payload,
            enum_class_payload,
            leaf::e_errno,
            leaf::error_info const & unmatched )
        {
            std::ostringstream st;
            st << unmatched;
            std::string s = st.str();
            BOOST_TEST_NE(s.find("leaf::error_info: Error ID = "), s.npos);
            BOOST_TEST_NE(s.find("Exception dynamic type: "), s.npos);
            BOOST_TEST_NE(s.find("std::exception::what(): my_exception"), s.npos);
            std::cout << s;
        } );

    std::cout << __LINE__  << " ----\n";

    leaf::try_catch(
        []
        {
            BOOST_LEAF_THROW_EXCEPTION( my_exception(),
                printable_info_printable_payload(),
                printable_info_non_printable_payload(),
                non_printable_info_printable_payload(),
                non_printable_info_non_printable_payload(),
                enum_class_payload{},
                unexpected_test<1>{1},
                unexpected_test<2>{2},
                leaf::e_errno{ENOENT} );
        },
        [](
            leaf::e_source_location,
            printable_info_printable_payload,
            printable_info_non_printable_payload,
            non_printable_info_printable_payload,
            non_printable_info_non_printable_payload,
            enum_class_payload,
            leaf::e_errno,
            leaf::diagnostic_info const & unmatched )
        {
            std::ostringstream st;
            st << unmatched;
            std::string s = st.str();
#if BOOST_LEAF_DIAGNOSTICS
            BOOST_TEST_NE(s.find("leaf::diagnostic_info for Error ID = "), s.npos);
            BOOST_TEST_NE(s.find("Exception dynamic type: "), s.npos);
            BOOST_TEST_NE(s.find("std::exception::what(): my_exception"), s.npos);
            BOOST_TEST_NE(s.find("e_source_location"), s.npos);
            BOOST_TEST_NE(s.find("*** printable_info_printable_payload printed printable_payload ***"), s.npos);
            BOOST_TEST_NE(s.find("*** printable_info_non_printable_payload ***"), s.npos);
            BOOST_TEST_NE(s.find(": printed printable_payload"), s.npos);
            BOOST_TEST_NE(s.find(": {Non-Printable}"), s.npos);
            BOOST_TEST_NE(s.find("enum_class_payload"), s.npos);
            BOOST_TEST_NE(s.find("Detected 2 attempts"), s.npos);
            BOOST_TEST_NE(s.find("unexpected_test<1>"), s.npos);
            BOOST_TEST_EQ(s.find("unexpected_test<2>"), s.npos);
#else
            BOOST_TEST_NE(s.find("leaf::diagnostic_info requires #define BOOST_LEAF_DIAGNOSTICS 1"), s.npos);
            BOOST_TEST_NE(s.find("leaf::error_info: Error ID = "), s.npos);
            BOOST_TEST_NE(s.find("Exception dynamic type: "), s.npos);
            BOOST_TEST_NE(s.find("std::exception::what(): my_exception"), s.npos);
#endif
            std::cout << s;
        } );

    std::cout << __LINE__  << " ----\n";

    leaf::try_catch(
        []
        {
            BOOST_LEAF_THROW_EXCEPTION( my_exception(),
                printable_info_printable_payload(),
                printable_info_non_printable_payload(),
                non_printable_info_printable_payload(),
                non_printable_info_non_printable_payload(),
                enum_class_payload{},
                unexpected_test<1>{1},
                unexpected_test<2>{2},
                leaf::e_errno{ENOENT} );
        },
        [](
            leaf::e_source_location,
            printable_info_printable_payload,
            printable_info_non_printable_payload,
            non_printable_info_printable_payload,
            non_printable_info_non_printable_payload,
            enum_class_payload,
            leaf::e_errno,
            leaf::verbose_diagnostic_info const & di )
        {
            std::ostringstream st;
            st << di;
            std::string s = st.str();
#if BOOST_LEAF_DIAGNOSTICS
            BOOST_TEST_NE(s.find("leaf::verbose_diagnostic_info for Error ID = "), s.npos);
            BOOST_TEST_NE(s.find("Exception dynamic type: "), s.npos);
            BOOST_TEST_NE(s.find("std::exception::what(): my_exception"), s.npos);
            BOOST_TEST_NE(s.find("e_source_location"), s.npos);
            BOOST_TEST_NE(s.find("*** printable_info_printable_payload printed printable_payload ***"), s.npos);
            BOOST_TEST_NE(s.find("*** printable_info_non_printable_payload ***"), s.npos);
            BOOST_TEST_NE(s.find(": printed printable_payload"), s.npos);
            BOOST_TEST_NE(s.find(": {Non-Printable}"), s.npos);
            BOOST_TEST_NE(s.find("enum_class_payload"), s.npos);
            BOOST_TEST_NE(s.find("Unhandled error objects:"), s.npos);
            BOOST_TEST_NE(s.find("unexpected_test<1>"), s.npos);
            BOOST_TEST_NE(s.find("unexpected_test<2>"), s.npos);
            BOOST_TEST_NE(s.find(": 1"), s.npos);
            BOOST_TEST_NE(s.find(": 2"), s.npos);
#else
            BOOST_TEST_NE(s.find("leaf::verbose_diagnostic_info requires #define BOOST_LEAF_DIAGNOSTICS 1"), s.npos);
            BOOST_TEST_NE(s.find("leaf::error_info: Error ID = "), s.npos);
            BOOST_TEST_NE(s.find("Exception dynamic type: "), s.npos);
            BOOST_TEST_NE(s.find("std::exception::what(): my_exception"), s.npos);
#endif
            std::cout << s;
        } );

#endif

    return boost::report_errors();
}
