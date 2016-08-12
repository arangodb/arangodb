/*
 *          Copyright Andrey Semashev 2007 - 2015.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   thread_specific.cpp
 * \author Andrey Semashev
 * \date   01.03.2008
 *
 * \brief  This header is the Boost.Log library implementation, see the library documentation
 *         at http://www.boost.org/doc/libs/release/libs/log/doc/html/index.html.
 */

#include <boost/log/detail/config.hpp>
#include <string>
#include <stdexcept>
#include <boost/log/exceptions.hpp>
#include <boost/log/detail/thread_specific.hpp>

#if !defined(BOOST_LOG_NO_THREADS)

#if defined(BOOST_THREAD_PLATFORM_WIN32)

#include <windows.h>
#include <boost/log/detail/header.hpp>

namespace boost {

BOOST_LOG_OPEN_NAMESPACE

namespace aux {

thread_specific_base::thread_specific_base()
{
    m_Key = TlsAlloc();
    if (BOOST_UNLIKELY(m_Key == TLS_OUT_OF_INDEXES))
    {
        BOOST_LOG_THROW_DESCR(system_error, "TLS capacity depleted");
    }
}

thread_specific_base::~thread_specific_base()
{
    TlsFree(m_Key);
}

void* thread_specific_base::get_content() const
{
    return TlsGetValue(m_Key);
}

void thread_specific_base::set_content(void* value) const
{
    TlsSetValue(m_Key, value);
}

} // namespace aux

BOOST_LOG_CLOSE_NAMESPACE // namespace log

} // namespace boost

#elif defined(BOOST_THREAD_PLATFORM_PTHREAD)

#include <cstddef>
#include <pthread.h>
#include <boost/log/detail/header.hpp>

namespace boost {

BOOST_LOG_OPEN_NAMESPACE

namespace aux {

BOOST_LOG_ANONYMOUS_NAMESPACE {

//! Some portability magic to detect how to store the TLS key
template< typename KeyT, bool IsStoreableV = sizeof(KeyT) <= sizeof(void*) >
struct pthread_key_traits
{
    typedef KeyT pthread_key_type;

    static void allocate(void*& stg)
    {
        pthread_key_type* pkey = new pthread_key_type();
        const int res = pthread_key_create(pkey, NULL);
        if (BOOST_UNLIKELY(res != 0))
        {
            delete pkey;
            BOOST_LOG_THROW_DESCR(system_error, "TLS capacity depleted");
        }
        stg = pkey;
    }

    static void deallocate(void* stg)
    {
        pthread_key_type* pkey = static_cast< pthread_key_type* >(stg);
        pthread_key_delete(*pkey);
        delete pkey;
    }

    static void set_value(void* stg, void* value)
    {
        pthread_setspecific(*static_cast< pthread_key_type* >(stg), value);
    }

    static void* get_value(void* stg)
    {
        return pthread_getspecific(*static_cast< pthread_key_type* >(stg));
    }
};

template< typename KeyT >
struct pthread_key_traits< KeyT, true >
{
    typedef KeyT pthread_key_type;

    union pthread_key_caster
    {
        void* as_storage;
        pthread_key_type as_key;
    };

    static void allocate(void*& stg)
    {
        pthread_key_caster caster = {};
        const int res = pthread_key_create(&caster.as_key, NULL);
        if (BOOST_UNLIKELY(res != 0))
        {
            BOOST_LOG_THROW_DESCR(system_error, "TLS capacity depleted");
        }
        stg = caster.as_storage;
    }

    static void deallocate(void* stg)
    {
        pthread_key_caster caster;
        caster.as_storage = stg;
        pthread_key_delete(caster.as_key);
    }

    static void set_value(void* stg, void* value)
    {
        pthread_key_caster caster;
        caster.as_storage = stg;
        pthread_setspecific(caster.as_key, value);
    }

    static void* get_value(void* stg)
    {
        pthread_key_caster caster;
        caster.as_storage = stg;
        return pthread_getspecific(caster.as_key);
    }
};

template< typename KeyT >
struct pthread_key_traits< KeyT*, true >
{
    typedef KeyT* pthread_key_type;

    static void allocate(void*& stg)
    {
        pthread_key_type key = NULL;
        const int res = pthread_key_create(&key, NULL);
        if (BOOST_UNLIKELY(res != 0))
        {
            BOOST_LOG_THROW_DESCR(system_error, "TLS capacity depleted");
        }
        stg = static_cast< void* >(key);
    }

    static void deallocate(void* stg)
    {
        pthread_key_delete(static_cast< pthread_key_type >(stg));
    }

    static void set_value(void* stg, void* value)
    {
        pthread_setspecific(static_cast< pthread_key_type >(stg), value);
    }

    static void* get_value(void* stg)
    {
        return pthread_getspecific(static_cast< pthread_key_type >(stg));
    }
};

} // namespace

thread_specific_base::thread_specific_base()
{
    typedef pthread_key_traits< pthread_key_t > traits_t;
    traits_t::allocate(m_Key);
}

thread_specific_base::~thread_specific_base()
{
    typedef pthread_key_traits< pthread_key_t > traits_t;
    traits_t::deallocate(m_Key);
}

void* thread_specific_base::get_content() const
{
    typedef pthread_key_traits< pthread_key_t > traits_t;
    return traits_t::get_value(m_Key);
}

void thread_specific_base::set_content(void* value) const
{
    typedef pthread_key_traits< pthread_key_t > traits_t;
    traits_t::set_value(m_Key, value);
}

} // namespace aux

BOOST_LOG_CLOSE_NAMESPACE // namespace log

} // namespace boost

#else
#error Boost.Log: unsupported threading API
#endif

#include <boost/log/detail/footer.hpp>

#endif // !defined(BOOST_LOG_NO_THREADS)
