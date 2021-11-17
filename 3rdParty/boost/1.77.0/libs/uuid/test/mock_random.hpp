//
// Copyright (c) 2017 James E. King III
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
//   https://www.boost.org/LICENSE_1_0.txt)
//
// Mocks are used to test sad paths by forcing error responses
//

#include <boost/core/ignore_unused.hpp>
#include <boost/uuid/detail/random_provider_detect_platform.hpp>

#if defined(BOOST_UUID_TEST_RANDOM_MOCK)

#if defined(BOOST_UUID_RANDOM_PROVIDER_WINCRYPT) || defined(BOOST_UUID_RANDOM_PROVIDER_POSIX)
#define BOOST_UUID_TEST_RANDOM_MOCK_LINKAGE BOOST_SYMBOL_IMPORT
#else
#define BOOST_UUID_TEST_RANDOM_MOCK_LINKAGE
#endif

//! \returns  true  if the provider can be mocked - if not then the test
//!                 should skip negative testing
BOOST_UUID_TEST_RANDOM_MOCK_LINKAGE bool expectations_capable();

//! Ensure all expectations for calls were consumed.  This means the number
//! of expected calls was met.
//! \returns  true  if all expectations were met
BOOST_UUID_TEST_RANDOM_MOCK_LINKAGE bool expectations_met();

//! Set the response of the next mocked random/crypto call - builds up
//! a queue of responses.  If the queue empties and another call is made, 
//! the test will core.
//! \param[in]  success  true for success response, false for failure
BOOST_UUID_TEST_RANDOM_MOCK_LINKAGE void expect_next_call_success(bool success);

//! \returns  true  if the provider acquires a context
BOOST_UUID_TEST_RANDOM_MOCK_LINKAGE bool provider_acquires_context();

#if defined(BOOST_UUID_RANDOM_PROVIDER_ARC4RANDOM)

// arc4random cannot fail therefore it needs no mocking at all!

bool expectations_capable()
{
    return false;
}

bool expectations_met()
{
    throw std::logic_error("expectations not supported");
}

void expect_next_call_success(bool success)
{
    boost::ignore_unused(success);
    throw std::logic_error("expectations not supported");
}

bool provider_acquires_context()
{
    throw std::logic_error("expectations not supported");
}

#elif defined(BOOST_UUID_RANDOM_PROVIDER_BCRYPT)

#include <boost/winapi/bcrypt.hpp>
#include <deque>
std::deque<boost::winapi::NTSTATUS_> bcrypt_next_result;

bool expectations_capable()
{
    return true;
}

bool expectations_met()
{
    return bcrypt_next_result.empty();
}

void expect_next_call_success(bool success)
{
    bcrypt_next_result.push_back(success ? 0 : 17);
}

bool provider_acquires_context()
{
    return true;
}

boost::winapi::NTSTATUS_ BOOST_WINAPI_WINAPI_CC
BCryptOpenAlgorithmProvider(
    boost::winapi::BCRYPT_ALG_HANDLE_ *phAlgorithm,
    boost::winapi::LPCWSTR_           pszAlgId,
    boost::winapi::LPCWSTR_           pszImplementation,
    boost::winapi::DWORD_             dwFlags
)
{
    boost::ignore_unused(phAlgorithm);
    boost::ignore_unused(pszAlgId);
    boost::ignore_unused(pszImplementation);
    boost::ignore_unused(dwFlags);

    boost::winapi::NTSTATUS_ result = bcrypt_next_result.front();
    bcrypt_next_result.pop_front();
    return result;
}

boost::winapi::NTSTATUS_ BOOST_WINAPI_WINAPI_CC
BCryptGenRandom(
    boost::winapi::BCRYPT_ALG_HANDLE_ hAlgorithm,
    boost::winapi::PUCHAR_            pbBuffer,
    boost::winapi::ULONG_             cbBuffer,
    boost::winapi::ULONG_             dwFlags
)
{
    boost::ignore_unused(hAlgorithm);
    boost::ignore_unused(pbBuffer);
    boost::ignore_unused(cbBuffer);
    boost::ignore_unused(dwFlags);

    boost::winapi::NTSTATUS_ result = bcrypt_next_result.front();
    bcrypt_next_result.pop_front();
    return result;
}

// the implementation ignores the result of close because it
// happens in a destructor
boost::winapi::NTSTATUS_ BOOST_WINAPI_WINAPI_CC
BCryptCloseAlgorithmProvider(
    boost::winapi::BCRYPT_ALG_HANDLE_ hAlgorithm,
    boost::winapi::ULONG_             dwFlags
)
{
    boost::ignore_unused(hAlgorithm);
    boost::ignore_unused(dwFlags);
    return 0;
}

#elif defined(BOOST_UUID_RANDOM_PROVIDER_GETRANDOM)

#include <deque>
#include <unistd.h>
std::deque<bool> getrandom_next_result;

bool expectations_capable()
{
    return true;
}

bool expectations_met()
{
    return getrandom_next_result.empty();
}

void expect_next_call_success(bool success)
{
    getrandom_next_result.push_back(success);
}

bool provider_acquires_context()
{
    return false;
}

ssize_t mock_getrandom(void *buffer, size_t length, unsigned int flags)
{
    boost::ignore_unused(buffer);
    boost::ignore_unused(length);
    boost::ignore_unused(flags);

    bool success = getrandom_next_result.front();
    getrandom_next_result.pop_front();
    return success ? static_cast< ssize_t >(length) : static_cast< ssize_t >(-1);
}

#define BOOST_UUID_RANDOM_PROVIDER_GETRANDOM_IMPL_GETRANDOM ::mock_getrandom

#elif defined(BOOST_UUID_RANDOM_PROVIDER_GETENTROPY)

//
// This stubbing technique works on unix because of how the loader resolves
// functions.  Locally defined functions resolve first.
//

#include <deque>
#include <unistd.h>
std::deque<int> getentropy_next_result;

bool expectations_capable()
{
    return true;
}

bool expectations_met()
{
    return getentropy_next_result.empty();
}

void expect_next_call_success(bool success)
{
    getentropy_next_result.push_back(success ? 0 : -1);
}

bool provider_acquires_context()
{
    return false;
}

int getentropy(void *buffer, size_t length)
{
    boost::ignore_unused(buffer);
    boost::ignore_unused(length);

    int result = getentropy_next_result.front();
    getentropy_next_result.pop_front();
    return result;
}

#elif defined(BOOST_UUID_RANDOM_PROVIDER_POSIX)

#include <boost/numeric/conversion/cast.hpp>
#include <deque>
#include <stdexcept>
std::deque<bool> posix_next_result; // bool success

bool expectations_capable()
{
    return true;
}

bool expectations_met()
{
    return posix_next_result.empty();
}

void expect_next_call_success(bool success)
{
    posix_next_result.push_back(success);
}

bool provider_acquires_context()
{
    return true;
}

int mockopen(const char *fname, int flags)
{
    boost::ignore_unused(fname);
    boost::ignore_unused(flags);

    bool success = posix_next_result.front();
    posix_next_result.pop_front();
    return success ? 17 : -1;
}

ssize_t mockread(int fd, void *buf, size_t siz)
{
    boost::ignore_unused(fd);
    boost::ignore_unused(buf);

    // first call siz is 4, in a success case we return 1
    // forcing a second loop to come through size 3

    if (siz < 4) { return boost::numeric_cast<ssize_t>(siz); }
    if (siz > 4) { throw std::logic_error("unexpected siz"); }

    bool success = posix_next_result.front();
    posix_next_result.pop_front();
    return success ? 1 : -1;
}

#define BOOST_UUID_RANDOM_PROVIDER_POSIX_IMPL_OPEN mockopen
#define BOOST_UUID_RANDOM_PROVIDER_POSIX_IMPL_READ mockread

#elif defined(BOOST_UUID_RANDOM_PROVIDER_WINCRYPT)

// Nothing to declare, since the expectation methods were already
// defined as import, we will link against a mock library

#else

#error support needed here for testing

#endif

#endif // BOOST_UUID_TEST_RANDOM_MOCK
