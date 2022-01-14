//
// Copyright (c) 2017 James E. King III
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
//   https://www.boost.org/LICENSE_1_0.txt)
//
// The contents of this file are compiled into a loadable
// library that is used for mocking purposes so that the error
// paths in the random_provider implementations are exercised.
//

#include <boost/config.hpp>
#include <boost/core/ignore_unused.hpp>

#if defined(BOOST_WINDOWS)

#include <boost/winapi/basic_types.hpp>

// WinAPI is not currently set up well for building mocks, as
// the definitions of wincrypt APIs all use BOOST_SYMBOL_IMPORT
// therefore we cannot include it, but we need some of the types
// so they are defined here...
namespace boost {
namespace winapi {
    typedef ULONG_PTR_ HCRYPTPROV_;
}
}

// wincrypt has to be mocked through a DLL pretending to be
// the real thing as the official APIs use __declspec(dllimport)

#include <deque>
std::deque<boost::winapi::BOOL_> wincrypt_next_result;

BOOST_SYMBOL_EXPORT bool expectations_capable()
{
    return true;
}

BOOST_SYMBOL_EXPORT bool expectations_met()
{
    return wincrypt_next_result.empty();
}

BOOST_SYMBOL_EXPORT void expect_next_call_success(bool success)
{
    wincrypt_next_result.push_back(success ? 1 : 0);
}

BOOST_SYMBOL_EXPORT bool provider_acquires_context()
{
    return true;
}

extern "C" {

BOOST_SYMBOL_EXPORT
boost::winapi::BOOL_ BOOST_WINAPI_WINAPI_CC
CryptAcquireContextW(
    boost::winapi::HCRYPTPROV_ *phProv,
    boost::winapi::LPCWSTR_ szContainer,
    boost::winapi::LPCWSTR_ szProvider,
    boost::winapi::DWORD_ dwProvType,
    boost::winapi::DWORD_ dwFlags)
{
    boost::ignore_unused(phProv);
    boost::ignore_unused(szContainer);
    boost::ignore_unused(szProvider);
    boost::ignore_unused(dwProvType);
    boost::ignore_unused(dwFlags);

    boost::winapi::BOOL_ result = wincrypt_next_result.front();
    wincrypt_next_result.pop_front();
    return result;
}

BOOST_SYMBOL_EXPORT
boost::winapi::BOOL_ BOOST_WINAPI_WINAPI_CC
CryptGenRandom(
    boost::winapi::HCRYPTPROV_ hProv,
    boost::winapi::DWORD_ dwLen,
    boost::winapi::BYTE_ *pbBuffer)
{
    boost::ignore_unused(hProv);
    boost::ignore_unused(dwLen);
    boost::ignore_unused(pbBuffer);

    boost::winapi::BOOL_ result = wincrypt_next_result.front();
    wincrypt_next_result.pop_front();
    return result;
}

// the implementation ignores the result of close because it
// happens in a destructor
BOOST_SYMBOL_EXPORT
boost::winapi::BOOL_ BOOST_WINAPI_WINAPI_CC
CryptReleaseContext(
    boost::winapi::HCRYPTPROV_ hProv,
#if defined(_MSC_VER) && (_MSC_VER+0) >= 1500 && (_MSC_VER+0) < 1900 && BOOST_USE_NTDDI_VERSION < BOOST_WINAPI_NTDDI_WINXP
    // see winapi crypt.hpp for more details on why these differ...
    boost::winapi::ULONG_PTR_ dwFlags
#else
    boost::winapi::DWORD_ dwFlags
#endif
)
{
    boost::ignore_unused(hProv);
    boost::ignore_unused(dwFlags);
    return true;
}

} // end extern "C"

#endif
