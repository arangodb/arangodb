//  boost win32_test.cpp  -----------------------------------------------------//

//  Copyright 2010 Vicente J. Botet Escriba

//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt

//  See http://www.boost.org/libs/chrono for documentation.
#include <boost/chrono/config.hpp>
#include <boost/detail/lightweight_test.hpp>
#if defined(BOOST_CHRONO_WINDOWS_API) ||  defined(__CYGWIN__)

#include <boost/chrono/detail/static_assert.hpp>
#if !defined(BOOST_NO_CXX11_STATIC_ASSERT)
#define NOTHING ""
#endif

#include <boost/type_traits.hpp>
#include <boost/typeof/typeof.hpp>
#undef BOOST_USE_WINDOWS_H
#include <boost/winapi/basic_types.hpp>
#include <boost/winapi/time.hpp>
#include <windows.h>

void test() {
    {
    boost::winapi::LARGE_INTEGER_ a;
    LARGE_INTEGER b;
    BOOST_CHRONO_STATIC_ASSERT((
            sizeof(boost::winapi::LARGE_INTEGER_)==sizeof(LARGE_INTEGER)
        ), NOTHING, (boost::winapi::LARGE_INTEGER_, LARGE_INTEGER));
    BOOST_TEST((
            sizeof(a.QuadPart)==sizeof(b.QuadPart)
            ));
    BOOST_CHRONO_STATIC_ASSERT((
            offsetof(boost::winapi::LARGE_INTEGER_, QuadPart)==offsetof(LARGE_INTEGER, QuadPart)
        ), NOTHING, (boost::winapi::LARGE_INTEGER_, LARGE_INTEGER));
    BOOST_CHRONO_STATIC_ASSERT((
            boost::is_same<
                    BOOST_TYPEOF(a.QuadPart),
                    BOOST_TYPEOF(b.QuadPart)
                >::value
        ), NOTHING, (boost::winapi::LARGE_INTEGER_, LARGE_INTEGER));
    }

    BOOST_CHRONO_STATIC_ASSERT((
            sizeof(boost::winapi::BOOL_)==sizeof(BOOL)
        ), NOTHING, (boost::winapi::BOOL_, BOOL));
    BOOST_CHRONO_STATIC_ASSERT((
            boost::is_same<boost::winapi::BOOL_,BOOL>::value
        ), NOTHING, (boost::winapi::BOOL_, BOOL));

    BOOST_CHRONO_STATIC_ASSERT((
            sizeof(boost::winapi::DWORD_)==sizeof(DWORD)
        ), NOTHING, (boost::winapi::DWORD_, DWORD));
    BOOST_CHRONO_STATIC_ASSERT((
            boost::is_same<boost::winapi::DWORD_,DWORD>::value
        ), NOTHING, (boost::winapi::DWORD_, DWORD));

    BOOST_CHRONO_STATIC_ASSERT((
            sizeof(boost::winapi::HANDLE_)==sizeof(HANDLE)
        ), NOTHING, (boost::winapi::HANDLE_, HANDLE));
    BOOST_CHRONO_STATIC_ASSERT((
            boost::is_same<boost::winapi::HANDLE_,HANDLE>::value
        ), NOTHING, (boost::winapi::HANDLE_, HANDLE));

    BOOST_CHRONO_STATIC_ASSERT((
            sizeof(boost::winapi::LONG_)==sizeof(LONG)
        ), NOTHING, (boost::winapi::LONG_, LONG));
    BOOST_CHRONO_STATIC_ASSERT((
            boost::is_same<boost::winapi::LONG_,LONG>::value
        ), NOTHING, (boost::winapi::LONG_, LONG));

    BOOST_CHRONO_STATIC_ASSERT((
            sizeof(boost::winapi::LONGLONG_)==sizeof(LONGLONG)
        ), NOTHING, (boost::winapi::LONGLONG_, LONGLONG));
    BOOST_CHRONO_STATIC_ASSERT((
            boost::is_same<boost::winapi::LONGLONG_,LONGLONG>::value
        ), NOTHING, (boost::winapi::LONGLONG_, LONGLONG));

    BOOST_CHRONO_STATIC_ASSERT((
            sizeof(boost::winapi::ULONG_PTR_)==sizeof(ULONG_PTR)
        ), NOTHING, (boost::winapi::ULONG_PTR_, ULONG_PTR));
    BOOST_CHRONO_STATIC_ASSERT((
            boost::is_same<boost::winapi::ULONG_PTR_,ULONG_PTR>::value
        ), NOTHING, (boost::winapi::ULONG_PTR_, ULONG_PTR));
        
    BOOST_CHRONO_STATIC_ASSERT((
            sizeof(boost::winapi::PLARGE_INTEGER_)==sizeof(PLARGE_INTEGER)
        ), NOTHING, (boost::winapi::PLARGE_INTEGER_, PLARGE_INTEGER));
    //~ BOOST_CHRONO_STATIC_ASSERT((
            //~ boost::is_same<boost::winapi::PLARGE_INTEGER_,PLARGE_INTEGER>::value
        //~ ), NOTHING, (boost::winapi::PLARGE_INTEGER_, PLARGE_INTEGER));
        
    {
        BOOST_CHRONO_STATIC_ASSERT((
                sizeof(boost::winapi::FILETIME_)==sizeof(FILETIME)
            ), NOTHING, (boost::winapi::FILETIME_, FILETIME));
        
        BOOST_CHRONO_STATIC_ASSERT((
                sizeof(boost::winapi::PFILETIME_)==sizeof(PFILETIME)
            ), NOTHING, (boost::winapi::PFILETIME_, PFILETIME));
        

        boost::winapi::FILETIME_ a;
        FILETIME b;
        BOOST_TEST((
                sizeof(a.dwLowDateTime)==sizeof(b.dwLowDateTime)
                ));
        BOOST_TEST((
                sizeof(a.dwHighDateTime)==sizeof(b.dwHighDateTime)
                ));
        BOOST_CHRONO_STATIC_ASSERT((
                offsetof(boost::winapi::FILETIME_, dwLowDateTime)==offsetof(FILETIME, dwLowDateTime)
            ), NOTHING, (boost::winapi::FILETIME_, FILETIME));
        BOOST_CHRONO_STATIC_ASSERT((
                offsetof(boost::winapi::FILETIME_, dwHighDateTime)==offsetof(FILETIME, dwHighDateTime)
            ), NOTHING, (boost::winapi::FILETIME_, FILETIME));
        BOOST_CHRONO_STATIC_ASSERT((
            boost::is_same<
                    BOOST_TYPEOF(a.dwLowDateTime),
                    BOOST_TYPEOF(b.dwLowDateTime)
                >::value
        ), NOTHING, (boost::winapi::FILETIME_, FILETIME));
        BOOST_CHRONO_STATIC_ASSERT((
            boost::is_same<
                    BOOST_TYPEOF(a.dwHighDateTime),
                    BOOST_TYPEOF(b.dwHighDateTime)
                >::value
        ), NOTHING, (boost::winapi::FILETIME_, FILETIME));

    }

//    BOOST_CHRONO_STATIC_ASSERT((
//            GetLastError==boost::winapi::::GetLastError
//        ), NOTHING, ());

}
#else
void test() {
}
#endif
int main(  )
{
    test();

  return boost::report_errors();
}

