#ifndef BOOST_WIN32_THREAD_PRIMITIVES_HPP
#define BOOST_WIN32_THREAD_PRIMITIVES_HPP

//  win32_thread_primitives.hpp
//
//  (C) Copyright 2005-7 Anthony Williams
//  (C) Copyright 2007 David Deakins
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/thread/detail/config.hpp>
#include <boost/predef/platform.h>
#include <boost/throw_exception.hpp>
#include <boost/assert.hpp>
#include <boost/thread/exceptions.hpp>
#include <boost/detail/interlocked.hpp>
#include <boost/detail/winapi/config.hpp>

#include <boost/detail/winapi/semaphore.hpp>
#include <boost/detail/winapi/dll.hpp>
#include <boost/detail/winapi/system.hpp>
#include <boost/detail/winapi/time.hpp>
#include <boost/detail/winapi/event.hpp>
#include <boost/detail/winapi/thread.hpp>
#include <boost/detail/winapi/get_current_thread.hpp>
#include <boost/detail/winapi/get_current_thread_id.hpp>
#include <boost/detail/winapi/get_current_process.hpp>
#include <boost/detail/winapi/get_current_process_id.hpp>
#include <boost/detail/winapi/wait.hpp>
#include <boost/detail/winapi/handles.hpp>
#include <boost/detail/winapi/access_rights.hpp>

//#include <boost/detail/winapi/synchronization.hpp>
#include <boost/thread/win32/interlocked_read.hpp>
#include <algorithm>

#if BOOST_PLAT_WINDOWS_RUNTIME
#include <thread>
#endif

namespace boost
{
    namespace detail
    {
        namespace win32
        {
            typedef ::boost::detail::winapi::HANDLE_ handle;
            typedef ::boost::detail::winapi::SYSTEM_INFO_ system_info;
            typedef unsigned __int64 ticks_type;
            typedef ::boost::detail::winapi::FARPROC_ farproc_t;
            unsigned const infinite=::boost::detail::winapi::INFINITE_;
            unsigned const timeout=::boost::detail::winapi::WAIT_TIMEOUT_;
            handle const invalid_handle_value=::boost::detail::winapi::INVALID_HANDLE_VALUE_;
            unsigned const event_modify_state=::boost::detail::winapi::EVENT_MODIFY_STATE_;
            unsigned const synchronize=::boost::detail::winapi::SYNCHRONIZE_;
            unsigned const wait_abandoned=::boost::detail::winapi::WAIT_ABANDONED_;
            unsigned const create_event_initial_set = 0x00000002;
            unsigned const create_event_manual_reset = 0x00000001;
            unsigned const event_all_access = ::boost::detail::winapi::EVENT_ALL_ACCESS_;
            unsigned const semaphore_all_access = boost::detail::winapi::SEMAPHORE_ALL_ACCESS_;
        }
    }
}

#include <boost/config/abi_prefix.hpp>

namespace boost
{
    namespace detail
    {
        namespace win32
        {
            namespace detail { typedef ticks_type (__stdcall *gettickcount64_t)(); }
#if !BOOST_PLAT_WINDOWS_RUNTIME
            extern "C"
            {
#ifdef _MSC_VER
                long _InterlockedCompareExchange(long volatile *, long, long);
#pragma intrinsic(_InterlockedCompareExchange)
#elif defined(__MINGW64_VERSION_MAJOR)
                long _InterlockedCompareExchange(long volatile *, long, long);
#else
                // Mingw doesn't provide intrinsics
#define _InterlockedCompareExchange InterlockedCompareExchange
#endif
            }
            // Borrowed from https://stackoverflow.com/questions/8211820/userland-interrupt-timer-access-such-as-via-kequeryinterrupttime-or-similar
            inline ticks_type __stdcall GetTickCount64emulation()
            {
                static long count = -1l;
                unsigned long previous_count, current_tick32, previous_count_zone, current_tick32_zone;
                ticks_type current_tick64;

                previous_count = (unsigned long) boost::detail::interlocked_read_acquire(&count);
                current_tick32 = ::boost::detail::winapi::GetTickCount();

                if(previous_count == (unsigned long)-1l)
                {
                    // count has never been written
                    unsigned long initial_count;
                    initial_count = current_tick32 >> 28;
                    previous_count = (unsigned long) _InterlockedCompareExchange(&count, (long)initial_count, -1l);

                    current_tick64 = initial_count;
                    current_tick64 <<= 28;
                    current_tick64 += current_tick32 & 0x0FFFFFFF;
                    return current_tick64;
                }

                previous_count_zone = previous_count & 15;
                current_tick32_zone = current_tick32 >> 28;

                if(current_tick32_zone == previous_count_zone)
                {
                    // The top four bits of the 32-bit tick count haven't changed since count was last written.
                    current_tick64 = previous_count;
                    current_tick64 <<= 28;
                    current_tick64 += current_tick32 & 0x0FFFFFFF;
                    return current_tick64;
                }

                if(current_tick32_zone == previous_count_zone + 1 || (current_tick32_zone == 0 && previous_count_zone == 15))
                {
                    // The top four bits of the 32-bit tick count have been incremented since count was last written.
                    unsigned long new_count = previous_count + 1;
                    _InterlockedCompareExchange(&count, (long)new_count, (long)previous_count);
                    current_tick64 = new_count;
                    current_tick64 <<= 28;
                    current_tick64 += current_tick32 & 0x0FFFFFFF;
                    return current_tick64;
                }

                // Oops, we weren't called often enough, we're stuck
                return 0xFFFFFFFF;
            }
#else
#endif
            inline detail::gettickcount64_t GetTickCount64_()
            {
                static detail::gettickcount64_t gettickcount64impl;
                if(gettickcount64impl)
                    return gettickcount64impl;

                // GetTickCount and GetModuleHandle are not allowed in the Windows Runtime,
                // and kernel32 isn't used in Windows Phone.
#if BOOST_PLAT_WINDOWS_RUNTIME
                gettickcount64impl = &::boost::detail::winapi::GetTickCount64;
#else
                farproc_t addr=GetProcAddress(
#if !defined(BOOST_NO_ANSI_APIS)
                    ::boost::detail::winapi::GetModuleHandleA("KERNEL32.DLL"),
#else
                    ::boost::detail::winapi::GetModuleHandleW(L"KERNEL32.DLL"),
#endif
                    "GetTickCount64");
                if(addr)
                    gettickcount64impl=(detail::gettickcount64_t) addr;
                else
                    gettickcount64impl=&GetTickCount64emulation;
#endif
                return gettickcount64impl;
            }

            enum event_type
            {
                auto_reset_event=false,
                manual_reset_event=true
            };

            enum initial_event_state
            {
                event_initially_reset=false,
                event_initially_set=true
            };

            inline handle create_event(
#if !defined(BOOST_NO_ANSI_APIS)
                const char *mutex_name,
#else
                const wchar_t *mutex_name,
#endif
                event_type type,
                initial_event_state state)
            {
#if !defined(BOOST_NO_ANSI_APIS)
                handle const res = ::boost::detail::winapi::CreateEventA(0, type, state, mutex_name);
#elif BOOST_USE_WINAPI_VERSION < BOOST_WINAPI_VERSION_VISTA
                handle const res = ::boost::detail::winapi::CreateEventW(0, type, state, mutex_name);
#else
                handle const res = ::boost::detail::winapi::CreateEventExW(
                    0,
                    mutex_name,
                    type ? create_event_manual_reset : 0 | state ? create_event_initial_set : 0,
                    event_all_access);
#endif
                return res;
            }

            inline handle create_anonymous_event(event_type type,initial_event_state state)
            {
                handle const res = create_event(0, type, state);
                if(!res)
                {
                    boost::throw_exception(thread_resource_error());
                }
                return res;
            }

            inline handle create_anonymous_semaphore_nothrow(long initial_count,long max_count)
            {
#if !defined(BOOST_NO_ANSI_APIS)
                handle const res=::boost::detail::winapi::CreateSemaphoreA(0,initial_count,max_count,0);
#else
#if BOOST_USE_WINAPI_VERSION < BOOST_WINAPI_VERSION_VISTA
                handle const res=::boost::detail::winapi::CreateSemaphoreEx(0,initial_count,max_count,0,0);
#else
                handle const res=::boost::detail::winapi::CreateSemaphoreExW(0,initial_count,max_count,0,0,semaphore_all_access);
#endif
#endif
                return res;
            }

            inline handle create_anonymous_semaphore(long initial_count,long max_count)
            {
                handle const res=create_anonymous_semaphore_nothrow(initial_count,max_count);
                if(!res)
                {
                    boost::throw_exception(thread_resource_error());
                }
                return res;
            }

            inline handle duplicate_handle(handle source)
            {
                handle const current_process=::boost::detail::winapi::GetCurrentProcess();
                long const same_access_flag=2;
                handle new_handle=0;
                bool const success=::boost::detail::winapi::DuplicateHandle(current_process,source,current_process,&new_handle,0,false,same_access_flag)!=0;
                if(!success)
                {
                    boost::throw_exception(thread_resource_error());
                }
                return new_handle;
            }

            inline void release_semaphore(handle semaphore,long count)
            {
                BOOST_VERIFY(::boost::detail::winapi::ReleaseSemaphore(semaphore,count,0)!=0);
            }

            inline void get_system_info(system_info *info)
            {
#if BOOST_PLAT_WINDOWS_RUNTIME
                ::boost::detail::winapi::GetNativeSystemInfo(info);
#else
                ::boost::detail::winapi::GetSystemInfo(info);
#endif
            }

            inline void sleep(unsigned long milliseconds)
            {
                if(milliseconds == 0)
                {
#if BOOST_PLAT_WINDOWS_RUNTIME
                    std::this_thread::yield();
#else
                    ::boost::detail::winapi::Sleep(0);
#endif
                }
                else
                {
#if BOOST_PLAT_WINDOWS_RUNTIME
                    ::boost::detail::winapi::WaitForSingleObjectEx(::boost::detail::winapi::GetCurrentThread(), milliseconds, 0);
#else
                    ::boost::detail::winapi::Sleep(milliseconds);
#endif
                }
            }

#if BOOST_PLAT_WINDOWS_RUNTIME
            class BOOST_THREAD_DECL scoped_winrt_thread
            {
            public:
                scoped_winrt_thread() : m_completionHandle(invalid_handle_value)
                {}

                ~scoped_winrt_thread()
                {
                    if (m_completionHandle != ::boost::detail::win32::invalid_handle_value)
                    {
                        ::boost::detail::winapi::CloseHandle(m_completionHandle);
                    }
                }

                typedef unsigned(__stdcall * thread_func)(void *);
                bool start(thread_func address, void *parameter, unsigned int *thrdId);

                handle waitable_handle() const
                {
                    BOOST_ASSERT(m_completionHandle != ::boost::detail::win32::invalid_handle_value);
                    return m_completionHandle;
                }

            private:
                handle m_completionHandle;
            };
#endif
            class BOOST_THREAD_DECL handle_manager
            {
            private:
                handle handle_to_manage;
                handle_manager(handle_manager&);
                handle_manager& operator=(handle_manager&);

                void cleanup()
                {
                    if(handle_to_manage && handle_to_manage!=invalid_handle_value)
                    {
                        BOOST_VERIFY(::boost::detail::winapi::CloseHandle(handle_to_manage));
                    }
                }

            public:
                explicit handle_manager(handle handle_to_manage_):
                    handle_to_manage(handle_to_manage_)
                {}
                handle_manager():
                    handle_to_manage(0)
                {}

                handle_manager& operator=(handle new_handle)
                {
                    cleanup();
                    handle_to_manage=new_handle;
                    return *this;
                }

                operator handle() const
                {
                    return handle_to_manage;
                }

                handle duplicate() const
                {
                    return duplicate_handle(handle_to_manage);
                }

                void swap(handle_manager& other)
                {
                    std::swap(handle_to_manage,other.handle_to_manage);
                }

                handle release()
                {
                    handle const res=handle_to_manage;
                    handle_to_manage=0;
                    return res;
                }

                bool operator!() const
                {
                    return !handle_to_manage;
                }

                ~handle_manager()
                {
                    cleanup();
                }
            };
        }
    }
}

#if defined(BOOST_MSVC) && (_MSC_VER>=1400)  && !defined(UNDER_CE)

namespace boost
{
    namespace detail
    {
        namespace win32
        {
#if _MSC_VER==1400
            extern "C" unsigned char _interlockedbittestandset(long *a,long b);
            extern "C" unsigned char _interlockedbittestandreset(long *a,long b);
#else
            extern "C" unsigned char _interlockedbittestandset(volatile long *a,long b);
            extern "C" unsigned char _interlockedbittestandreset(volatile long *a,long b);
#endif

#pragma intrinsic(_interlockedbittestandset)
#pragma intrinsic(_interlockedbittestandreset)

            inline bool interlocked_bit_test_and_set(long* x,long bit)
            {
                return _interlockedbittestandset(x,bit)!=0;
            }

            inline bool interlocked_bit_test_and_reset(long* x,long bit)
            {
                return _interlockedbittestandreset(x,bit)!=0;
            }

        }
    }
}
#define BOOST_THREAD_BTS_DEFINED
#elif (defined(BOOST_MSVC) || defined(BOOST_INTEL_WIN)) && defined(_M_IX86)
namespace boost
{
    namespace detail
    {
        namespace win32
        {
            inline bool interlocked_bit_test_and_set(long* x,long bit)
            {
#ifndef BOOST_INTEL_CXX_VERSION
                __asm {
                    mov eax,bit;
                    mov edx,x;
                    lock bts [edx],eax;
                    setc al;
                };
#else
                bool ret;
                __asm {
                    mov eax,bit
                    mov edx,x
                    lock bts [edx],eax
                    setc al
                    mov ret, al
                };
                return ret;

#endif
            }

            inline bool interlocked_bit_test_and_reset(long* x,long bit)
            {
#ifndef BOOST_INTEL_CXX_VERSION
                __asm {
                    mov eax,bit;
                    mov edx,x;
                    lock btr [edx],eax;
                    setc al;
                };
#else
                bool ret;
                __asm {
                    mov eax,bit
                    mov edx,x
                    lock btr [edx],eax
                    setc al
                    mov ret, al
                };
                return ret;

#endif
            }

        }
    }
}
#define BOOST_THREAD_BTS_DEFINED
#endif

#ifndef BOOST_THREAD_BTS_DEFINED

namespace boost
{
    namespace detail
    {
        namespace win32
        {
            inline bool interlocked_bit_test_and_set(long* x,long bit)
            {
                long const value=1<<bit;
                long old=*x;
                do
                {
                    long const current=BOOST_INTERLOCKED_COMPARE_EXCHANGE(x,old|value,old);
                    if(current==old)
                    {
                        break;
                    }
                    old=current;
                }
                while(true) ;
                return (old&value)!=0;
            }

            inline bool interlocked_bit_test_and_reset(long* x,long bit)
            {
                long const value=1<<bit;
                long old=*x;
                do
                {
                    long const current=BOOST_INTERLOCKED_COMPARE_EXCHANGE(x,old&~value,old);
                    if(current==old)
                    {
                        break;
                    }
                    old=current;
                }
                while(true) ;
                return (old&value)!=0;
            }
        }
    }
}
#endif

#include <boost/config/abi_suffix.hpp>

#endif
