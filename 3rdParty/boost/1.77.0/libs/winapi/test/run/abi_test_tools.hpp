/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   abi_test_tools.hpp
 * \author Andrey Semashev
 * \date   09.03.2018
 *
 * \brief  This file contains a set of ABI testing tools
 */

#ifndef BOOST_WINAPI_TEST_RUN_ABI_TEST_TOOLS_HPP_INCLUDED_
#define BOOST_WINAPI_TEST_RUN_ABI_TEST_TOOLS_HPP_INCLUDED_

#include <boost/winapi/config.hpp>
#include <boost/config.hpp>
#include <boost/current_function.hpp>
#include <boost/core/is_same.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <cstddef> // offsetof

//! The macro produces Boost.WinAPI equivalent for a Windows SDK constant or type name
#define BOOST_WINAPI_NAME(name)\
    boost::winapi::name

//! The macro tests that the given constant has the same value in Boost.WinAPI and Windows SDK
#define BOOST_WINAPI_TEST_CONSTANT(name)\
    BOOST_TEST_EQ(name, BOOST_WINAPI_NAME(name ## _))

//! The macro tests that the given type is the same in Boost.WinAPI and Windows SDK
#define BOOST_WINAPI_TEST_TYPE_SAME(name)\
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same< name, BOOST_WINAPI_NAME(name ## _) >))

//! The macro tests that the given type has the same size in Boost.WinAPI and Windows SDK
#define BOOST_WINAPI_TEST_TYPE_SIZE(name)\
    BOOST_TEST_EQ(sizeof(name), sizeof(BOOST_WINAPI_NAME(name ## _)))

#define BOOST_WINAPI_TEST_STRUCT_FIELD(r, names, field)\
    BOOST_TEST_EQ(sizeof(BOOST_PP_TUPLE_ELEM(2, 0, names)().field), sizeof(BOOST_PP_TUPLE_ELEM(2, 1, names)().field));\
    BOOST_TEST_EQ(offsetof(BOOST_PP_TUPLE_ELEM(2, 0, names), field), offsetof(BOOST_PP_TUPLE_ELEM(2, 1, names), field));

//! The macro tests that the structure has the same size, and its fields have the same size and offsets in Boost.WinAPI and Windows SDK
#define BOOST_WINAPI_TEST_STRUCT(name, fields)\
    BOOST_PP_SEQ_FOR_EACH(BOOST_WINAPI_TEST_STRUCT_FIELD, (name, BOOST_WINAPI_NAME(name ## _)), fields)\
    BOOST_TEST_EQ(sizeof(name), sizeof(BOOST_WINAPI_NAME(name ## _)))

#if defined(BOOST_MSVC)
#pragma warning(push, 3)
// conditional expression is constant
#pragma warning(disable: 4127)
#endif

template< typename Windows_SDK_Signature, typename BoostWinAPI_Signature >
inline void test_equal_signatures(Windows_SDK_Signature*, BoostWinAPI_Signature*, const char* test_name, const char* file, int line)
{
    // pass BOOST_CURRENT_FUNCTION here to include signature types in the error message
    boost::detail::test_impl(test_name, file, line, BOOST_CURRENT_FUNCTION,
        boost::core::is_same< Windows_SDK_Signature, BoostWinAPI_Signature >::value);
}

#if defined(BOOST_MSVC)
#pragma warning(pop)
#endif

/*!
 * \brief The macro tests that the given function signature is the same in Boost.WinAPI and Windows SDK
 *
 * If the signatures don't match, the test may fail to compile instead of failing at runtime. Depending on the compiler,
 * the error message may say that either there are multiple different declarations of an extern "C" function, or that
 * \c test_equal_signatures cannot be instantiated because of an unresolved overloaded function. This is because we currently
 * have forward declarations of Windows functions in the global namespace, so technically the declaration from Boost.WinAPI
 * and from Windows SDK are either two declarations of the same function (if they match) or declarations of two overloaded
 * functions (which is an error because both of them are extern "C"). Anyway, the test will fail if there's an error and
 * will pass if everything's fine, which is what we want.
 *
 * \note Do not use this macro to compare function signatures where we use our own structures binary-compatible with Windows SDK.
 *       Such functions are defined as inline wrappers in Boost.WinAPI headers and the wrappers will obviously not compare equal
 *       to the functions in Windows SDK. Currently, there's no way to test those function declarations because Boost.WinAPI
 *       declares Windows functions in the global namespace. They are implicitly tested by the compiler though, which should fail
 *       to compile if the declarations don't match.
 */
#if !defined(BOOST_NO_CXX11_DECLTYPE)
#define BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(name)\
    test_equal_signatures((decltype(&name))0, (decltype(&BOOST_WINAPI_NAME(name)))0, "BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(" #name ")", __FILE__, __LINE__)
#else
#define BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(name)\
    test_equal_signatures(&name, &BOOST_WINAPI_NAME(name), "BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(" #name ")", __FILE__, __LINE__)
#endif

#endif // BOOST_WINAPI_TEST_RUN_ABI_TEST_TOOLS_HPP_INCLUDED_
