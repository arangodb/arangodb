/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2011 Helge Bahmann
 * Copyright (c) 2013 Tim Blechmann
 * Copyright (c) 2014 Andrey Semashev
 */
/*!
 * \file   atomic/detail/atomic_template.hpp
 *
 * This header contains interface definition of \c atomic template.
 */

#ifndef BOOST_ATOMIC_DETAIL_ATOMIC_TEMPLATE_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_ATOMIC_TEMPLATE_HPP_INCLUDED_

#include <cstddef>
#include <boost/cstdint.hpp>
#include <boost/assert.hpp>
#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/bitwise_cast.hpp>
#include <boost/atomic/detail/operations_fwd.hpp>
#include <boost/atomic/detail/extra_operations_fwd.hpp>
#include <boost/atomic/detail/type_traits/is_signed.hpp>
#include <boost/atomic/detail/type_traits/is_integral.hpp>
#include <boost/atomic/detail/type_traits/is_function.hpp>
#include <boost/atomic/detail/type_traits/conditional.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

#if defined(BOOST_MSVC)
#pragma warning(push)
// 'boost::atomics::atomic<T>' : multiple assignment operators specified
#pragma warning(disable: 4522)
#endif

/*
 * IMPLEMENTATION NOTE: All interface functions MUST be declared with BOOST_FORCEINLINE,
 *                      see comment for convert_memory_order_to_gcc in ops_gcc_atomic.hpp.
 */

namespace boost {
namespace atomics {
namespace detail {

BOOST_FORCEINLINE BOOST_CONSTEXPR memory_order deduce_failure_order(memory_order order) BOOST_NOEXCEPT
{
    return order == memory_order_acq_rel ? memory_order_acquire : (order == memory_order_release ? memory_order_relaxed : order);
}

BOOST_FORCEINLINE BOOST_CONSTEXPR bool cas_failure_order_must_not_be_stronger_than_success_order(memory_order success_order, memory_order failure_order) BOOST_NOEXCEPT
{
    // 15 == (memory_order_seq_cst | memory_order_consume), see memory_order.hpp
    // Given the enum values we can test the strength of memory order requirements with this single condition.
    return (failure_order & 15u) <= (success_order & 15u);
}

template< typename T, bool IsFunction = boost::atomics::detail::is_function< T >::value >
struct classify_pointer
{
    typedef void* type;
};

template< typename T >
struct classify_pointer< T, true >
{
    typedef void type;
};

template< typename T, bool IsInt = boost::atomics::detail::is_integral< T >::value >
struct classify
{
    typedef void type;
};

template< typename T >
struct classify< T, true > { typedef int type; };

template< typename T >
struct classify< T*, false > { typedef typename classify_pointer< T >::type type; };

template< >
struct classify< void*, false > { typedef void type; };

template< >
struct classify< const void*, false > { typedef void type; };

template< >
struct classify< volatile void*, false > { typedef void type; };

template< >
struct classify< const volatile void*, false > { typedef void type; };

template< typename T, typename U >
struct classify< T U::*, false > { typedef void type; };

template< bool >
struct boolean_constant {};
typedef boolean_constant< true > true_constant;
typedef boolean_constant< false > false_constant;


template< typename T, typename Kind >
class base_atomic;

//! General template. Implementation for user-defined types, such as structs and enums, and pointers to non-object types
template< typename T >
class base_atomic< T, void >
{
public:
    typedef T value_type;

protected:
    typedef atomics::detail::operations< storage_size_of< value_type >::value, false > operations;
    typedef typename boost::atomics::detail::conditional< sizeof(value_type) <= sizeof(void*), value_type, value_type const& >::type value_arg_type;

public:
    typedef typename operations::storage_type storage_type;

private:
    typedef boolean_constant< sizeof(value_type) == sizeof(storage_type) > value_matches_storage;

protected:
    typename operations::aligned_storage_type m_storage;

public:
    BOOST_FORCEINLINE explicit base_atomic(value_arg_type v = value_type()) BOOST_NOEXCEPT : m_storage(atomics::detail::bitwise_cast< storage_type >(v))
    {
    }

    BOOST_FORCEINLINE void store(value_arg_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        BOOST_ASSERT(order != memory_order_consume);
        BOOST_ASSERT(order != memory_order_acquire);
        BOOST_ASSERT(order != memory_order_acq_rel);

        operations::store(m_storage.value, atomics::detail::bitwise_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE value_type load(memory_order order = memory_order_seq_cst) const volatile BOOST_NOEXCEPT
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        return atomics::detail::bitwise_cast< value_type >(operations::load(m_storage.value, order));
    }

    BOOST_FORCEINLINE value_type exchange(value_arg_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return atomics::detail::bitwise_cast< value_type >(operations::exchange(m_storage.value, atomics::detail::bitwise_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE bool compare_exchange_strong(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order) volatile BOOST_NOEXCEPT
    {
        BOOST_ASSERT(failure_order != memory_order_release);
        BOOST_ASSERT(failure_order != memory_order_acq_rel);
        BOOST_ASSERT(cas_failure_order_must_not_be_stronger_than_success_order(success_order, failure_order));

        return compare_exchange_strong_impl(expected, desired, success_order, failure_order, value_matches_storage());
    }

    BOOST_FORCEINLINE bool compare_exchange_strong(value_type& expected, value_arg_type desired, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return compare_exchange_strong(expected, desired, order, atomics::detail::deduce_failure_order(order));
    }

    BOOST_FORCEINLINE bool compare_exchange_weak(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order) volatile BOOST_NOEXCEPT
    {
        BOOST_ASSERT(failure_order != memory_order_release);
        BOOST_ASSERT(failure_order != memory_order_acq_rel);
        BOOST_ASSERT(cas_failure_order_must_not_be_stronger_than_success_order(success_order, failure_order));

        return compare_exchange_weak_impl(expected, desired, success_order, failure_order, value_matches_storage());
    }

    BOOST_FORCEINLINE bool compare_exchange_weak(value_type& expected, value_arg_type desired, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return compare_exchange_weak(expected, desired, order, atomics::detail::deduce_failure_order(order));
    }

    BOOST_DELETED_FUNCTION(base_atomic(base_atomic const&))
    BOOST_DELETED_FUNCTION(base_atomic& operator=(base_atomic const&))

private:
    BOOST_FORCEINLINE bool compare_exchange_strong_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, true_constant) volatile BOOST_NOEXCEPT
    {
#if defined(BOOST_ATOMIC_DETAIL_STORAGE_TYPE_MAY_ALIAS)
        return operations::compare_exchange_strong(m_storage.value, reinterpret_cast< storage_type& >(expected), atomics::detail::bitwise_cast< storage_type >(desired), success_order, failure_order);
#else
        return compare_exchange_strong_impl(expected, desired, success_order, failure_order, false_constant());
#endif
    }

    BOOST_FORCEINLINE bool compare_exchange_strong_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, false_constant) volatile BOOST_NOEXCEPT
    {
        storage_type old_value = atomics::detail::bitwise_cast< storage_type >(expected);
        const bool res = operations::compare_exchange_strong(m_storage.value, old_value, atomics::detail::bitwise_cast< storage_type >(desired), success_order, failure_order);
        expected = atomics::detail::bitwise_cast< value_type >(old_value);
        return res;
    }

    BOOST_FORCEINLINE bool compare_exchange_weak_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, true_constant) volatile BOOST_NOEXCEPT
    {
#if defined(BOOST_ATOMIC_DETAIL_STORAGE_TYPE_MAY_ALIAS)
        return operations::compare_exchange_weak(m_storage.value, reinterpret_cast< storage_type& >(expected), atomics::detail::bitwise_cast< storage_type >(desired), success_order, failure_order);
#else
        return compare_exchange_weak_impl(expected, desired, success_order, failure_order, false_constant());
#endif
    }

    BOOST_FORCEINLINE bool compare_exchange_weak_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, false_constant) volatile BOOST_NOEXCEPT
    {
        storage_type old_value = atomics::detail::bitwise_cast< storage_type >(expected);
        const bool res = operations::compare_exchange_weak(m_storage.value, old_value, atomics::detail::bitwise_cast< storage_type >(desired), success_order, failure_order);
        expected = atomics::detail::bitwise_cast< value_type >(old_value);
        return res;
    }
};


//! Implementation for integers
template< typename T >
class base_atomic< T, int >
{
public:
    typedef T value_type;
    typedef T difference_type;

protected:
    typedef atomics::detail::operations< storage_size_of< value_type >::value, boost::atomics::detail::is_signed< T >::value > operations;
    typedef atomics::detail::extra_operations< operations, operations::storage_size, operations::is_signed > extra_operations;
    typedef value_type value_arg_type;

public:
    typedef typename operations::storage_type storage_type;

protected:
    typename operations::aligned_storage_type m_storage;

public:
    BOOST_DEFAULTED_FUNCTION(base_atomic(), {})
    BOOST_CONSTEXPR explicit base_atomic(value_type v) BOOST_NOEXCEPT : m_storage(v) {}

    // Standard methods
    BOOST_FORCEINLINE void store(value_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        BOOST_ASSERT(order != memory_order_consume);
        BOOST_ASSERT(order != memory_order_acquire);
        BOOST_ASSERT(order != memory_order_acq_rel);

        operations::store(m_storage.value, static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE value_type load(memory_order order = memory_order_seq_cst) const volatile BOOST_NOEXCEPT
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        return static_cast< value_type >(operations::load(m_storage.value, order));
    }

    BOOST_FORCEINLINE value_type fetch_add(difference_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return static_cast< value_type >(operations::fetch_add(m_storage.value, static_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE value_type fetch_sub(difference_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return static_cast< value_type >(operations::fetch_sub(m_storage.value, static_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE value_type exchange(value_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return static_cast< value_type >(operations::exchange(m_storage.value, static_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE bool compare_exchange_strong(value_type& expected, value_type desired, memory_order success_order, memory_order failure_order) volatile BOOST_NOEXCEPT
    {
        BOOST_ASSERT(failure_order != memory_order_release);
        BOOST_ASSERT(failure_order != memory_order_acq_rel);
        BOOST_ASSERT(cas_failure_order_must_not_be_stronger_than_success_order(success_order, failure_order));

#if defined(BOOST_ATOMIC_DETAIL_STORAGE_TYPE_MAY_ALIAS)
        return operations::compare_exchange_strong(m_storage.value, reinterpret_cast< storage_type& >(expected), static_cast< storage_type >(desired), success_order, failure_order);
#else
        storage_type old_value = static_cast< storage_type >(expected);
        const bool res = operations::compare_exchange_strong(m_storage.value, old_value, static_cast< storage_type >(desired), success_order, failure_order);
        expected = static_cast< value_type >(old_value);
        return res;
#endif
    }

    BOOST_FORCEINLINE bool compare_exchange_strong(value_type& expected, value_type desired, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return compare_exchange_strong(expected, desired, order, atomics::detail::deduce_failure_order(order));
    }

    BOOST_FORCEINLINE bool compare_exchange_weak(value_type& expected, value_type desired, memory_order success_order, memory_order failure_order) volatile BOOST_NOEXCEPT
    {
        BOOST_ASSERT(failure_order != memory_order_release);
        BOOST_ASSERT(failure_order != memory_order_acq_rel);
        BOOST_ASSERT(cas_failure_order_must_not_be_stronger_than_success_order(success_order, failure_order));

#if defined(BOOST_ATOMIC_DETAIL_STORAGE_TYPE_MAY_ALIAS)
        return operations::compare_exchange_weak(m_storage.value, reinterpret_cast< storage_type& >(expected), static_cast< storage_type >(desired), success_order, failure_order);
#else
        storage_type old_value = static_cast< storage_type >(expected);
        const bool res = operations::compare_exchange_weak(m_storage.value, old_value, static_cast< storage_type >(desired), success_order, failure_order);
        expected = static_cast< value_type >(old_value);
        return res;
#endif
    }

    BOOST_FORCEINLINE bool compare_exchange_weak(value_type& expected, value_type desired, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return compare_exchange_weak(expected, desired, order, atomics::detail::deduce_failure_order(order));
    }

    BOOST_FORCEINLINE value_type fetch_and(value_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return static_cast< value_type >(operations::fetch_and(m_storage.value, static_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE value_type fetch_or(value_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return static_cast< value_type >(operations::fetch_or(m_storage.value, static_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE value_type fetch_xor(value_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return static_cast< value_type >(operations::fetch_xor(m_storage.value, static_cast< storage_type >(v), order));
    }

    // Boost.Atomic extensions
    BOOST_FORCEINLINE value_type fetch_negate(memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return static_cast< value_type >(extra_operations::fetch_negate(m_storage.value, order));
    }

    BOOST_FORCEINLINE value_type fetch_complement(memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return static_cast< value_type >(extra_operations::fetch_complement(m_storage.value, order));
    }

    BOOST_FORCEINLINE void opaque_add(difference_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        extra_operations::opaque_add(m_storage.value, static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE void opaque_sub(difference_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        extra_operations::opaque_sub(m_storage.value, static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE void opaque_negate(memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        extra_operations::opaque_negate(m_storage.value, order);
    }

    BOOST_FORCEINLINE void opaque_and(value_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        extra_operations::opaque_and(m_storage.value, static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE void opaque_or(value_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        extra_operations::opaque_or(m_storage.value, static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE void opaque_xor(value_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        extra_operations::opaque_xor(m_storage.value, static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE void opaque_complement(memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        extra_operations::opaque_complement(m_storage.value, order);
    }

    BOOST_FORCEINLINE bool add_and_test(difference_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return extra_operations::add_and_test(m_storage.value, static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE bool sub_and_test(difference_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return extra_operations::sub_and_test(m_storage.value, static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE bool and_and_test(value_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return extra_operations::and_and_test(m_storage.value, static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE bool or_and_test(value_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return extra_operations::or_and_test(m_storage.value, static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE bool xor_and_test(value_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return extra_operations::xor_and_test(m_storage.value, static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE bool bit_test_and_set(unsigned int bit_number, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        BOOST_ASSERT(bit_number < sizeof(value_type) * 8u);
        return extra_operations::bit_test_and_set(m_storage.value, bit_number, order);
    }

    BOOST_FORCEINLINE bool bit_test_and_reset(unsigned int bit_number, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        BOOST_ASSERT(bit_number < sizeof(value_type) * 8u);
        return extra_operations::bit_test_and_reset(m_storage.value, bit_number, order);
    }

    BOOST_FORCEINLINE bool bit_test_and_complement(unsigned int bit_number, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        BOOST_ASSERT(bit_number < sizeof(value_type) * 8u);
        return extra_operations::bit_test_and_complement(m_storage.value, bit_number, order);
    }

    // Operators
    BOOST_FORCEINLINE value_type operator++(int) volatile BOOST_NOEXCEPT
    {
        return fetch_add(1);
    }

    BOOST_FORCEINLINE value_type operator++() volatile BOOST_NOEXCEPT
    {
        return fetch_add(1) + 1;
    }

    BOOST_FORCEINLINE value_type operator--(int) volatile BOOST_NOEXCEPT
    {
        return fetch_sub(1);
    }

    BOOST_FORCEINLINE value_type operator--() volatile BOOST_NOEXCEPT
    {
        return fetch_sub(1) - 1;
    }

    BOOST_FORCEINLINE value_type operator+=(difference_type v) volatile BOOST_NOEXCEPT
    {
        return fetch_add(v) + v;
    }

    BOOST_FORCEINLINE value_type operator-=(difference_type v) volatile BOOST_NOEXCEPT
    {
        return fetch_sub(v) - v;
    }

    BOOST_FORCEINLINE value_type operator&=(value_type v) volatile BOOST_NOEXCEPT
    {
        return fetch_and(v) & v;
    }

    BOOST_FORCEINLINE value_type operator|=(value_type v) volatile BOOST_NOEXCEPT
    {
        return fetch_or(v) | v;
    }

    BOOST_FORCEINLINE value_type operator^=(value_type v) volatile BOOST_NOEXCEPT
    {
        return fetch_xor(v) ^ v;
    }

    BOOST_DELETED_FUNCTION(base_atomic(base_atomic const&))
    BOOST_DELETED_FUNCTION(base_atomic& operator=(base_atomic const&))
};

//! Implementation for bool
template< >
class base_atomic< bool, int >
{
public:
    typedef bool value_type;

protected:
    typedef atomics::detail::operations< 1u, false > operations;
    typedef value_type value_arg_type;

public:
    typedef operations::storage_type storage_type;

protected:
    operations::aligned_storage_type m_storage;

public:
    BOOST_DEFAULTED_FUNCTION(base_atomic(), {})
    BOOST_CONSTEXPR explicit base_atomic(value_type v) BOOST_NOEXCEPT : m_storage(v) {}

    // Standard methods
    BOOST_FORCEINLINE void store(value_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        BOOST_ASSERT(order != memory_order_consume);
        BOOST_ASSERT(order != memory_order_acquire);
        BOOST_ASSERT(order != memory_order_acq_rel);

        operations::store(m_storage.value, static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE value_type load(memory_order order = memory_order_seq_cst) const volatile BOOST_NOEXCEPT
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        return !!operations::load(m_storage.value, order);
    }

    BOOST_FORCEINLINE value_type exchange(value_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return !!operations::exchange(m_storage.value, static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE bool compare_exchange_strong(value_type& expected, value_type desired, memory_order success_order, memory_order failure_order) volatile BOOST_NOEXCEPT
    {
        BOOST_ASSERT(failure_order != memory_order_release);
        BOOST_ASSERT(failure_order != memory_order_acq_rel);
        BOOST_ASSERT(cas_failure_order_must_not_be_stronger_than_success_order(success_order, failure_order));

#if defined(BOOST_ATOMIC_DETAIL_STORAGE_TYPE_MAY_ALIAS)
        return operations::compare_exchange_strong(m_storage.value, reinterpret_cast< storage_type& >(expected), static_cast< storage_type >(desired), success_order, failure_order);
#else
        storage_type old_value = static_cast< storage_type >(expected);
        const bool res = operations::compare_exchange_strong(m_storage.value, old_value, static_cast< storage_type >(desired), success_order, failure_order);
        expected = !!old_value;
        return res;
#endif
    }

    BOOST_FORCEINLINE bool compare_exchange_strong(value_type& expected, value_type desired, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return compare_exchange_strong(expected, desired, order, atomics::detail::deduce_failure_order(order));
    }

    BOOST_FORCEINLINE bool compare_exchange_weak(value_type& expected, value_type desired, memory_order success_order, memory_order failure_order) volatile BOOST_NOEXCEPT
    {
        BOOST_ASSERT(failure_order != memory_order_release);
        BOOST_ASSERT(failure_order != memory_order_acq_rel);
        BOOST_ASSERT(cas_failure_order_must_not_be_stronger_than_success_order(success_order, failure_order));

#if defined(BOOST_ATOMIC_DETAIL_STORAGE_TYPE_MAY_ALIAS)
        return operations::compare_exchange_weak(m_storage.value, reinterpret_cast< storage_type& >(expected), static_cast< storage_type >(desired), success_order, failure_order);
#else
        storage_type old_value = static_cast< storage_type >(expected);
        const bool res = operations::compare_exchange_weak(m_storage.value, old_value, static_cast< storage_type >(desired), success_order, failure_order);
        expected = !!old_value;
        return res;
#endif
    }

    BOOST_FORCEINLINE bool compare_exchange_weak(value_type& expected, value_type desired, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return compare_exchange_weak(expected, desired, order, atomics::detail::deduce_failure_order(order));
    }

    BOOST_DELETED_FUNCTION(base_atomic(base_atomic const&))
    BOOST_DELETED_FUNCTION(base_atomic& operator=(base_atomic const&))
};


//! Implementation for pointers to object types
template< typename T >
class base_atomic< T*, void* >
{
public:
    typedef T* value_type;
    typedef std::ptrdiff_t difference_type;

protected:
    typedef atomics::detail::operations< storage_size_of< value_type >::value, false > operations;
    typedef atomics::detail::extra_operations< operations, operations::storage_size, operations::is_signed > extra_operations;
    typedef value_type value_arg_type;

public:
    typedef typename operations::storage_type storage_type;

protected:
    typename operations::aligned_storage_type m_storage;

public:
    BOOST_DEFAULTED_FUNCTION(base_atomic(), {})
    BOOST_FORCEINLINE explicit base_atomic(value_type const& v) BOOST_NOEXCEPT : m_storage(atomics::detail::bitwise_cast< storage_type >(v))
    {
    }

    // Standard methods
    BOOST_FORCEINLINE void store(value_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        BOOST_ASSERT(order != memory_order_consume);
        BOOST_ASSERT(order != memory_order_acquire);
        BOOST_ASSERT(order != memory_order_acq_rel);

        operations::store(m_storage.value, atomics::detail::bitwise_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE value_type load(memory_order order = memory_order_seq_cst) const volatile BOOST_NOEXCEPT
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        return atomics::detail::bitwise_cast< value_type >(operations::load(m_storage.value, order));
    }

    BOOST_FORCEINLINE value_type fetch_add(difference_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return atomics::detail::bitwise_cast< value_type >(operations::fetch_add(m_storage.value, static_cast< storage_type >(v * sizeof(T)), order));
    }

    BOOST_FORCEINLINE value_type fetch_sub(difference_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return atomics::detail::bitwise_cast< value_type >(operations::fetch_sub(m_storage.value, static_cast< storage_type >(v * sizeof(T)), order));
    }

    BOOST_FORCEINLINE value_type exchange(value_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return atomics::detail::bitwise_cast< value_type >(operations::exchange(m_storage.value, atomics::detail::bitwise_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE bool compare_exchange_strong(value_type& expected, value_type desired, memory_order success_order, memory_order failure_order) volatile BOOST_NOEXCEPT
    {
        BOOST_ASSERT(failure_order != memory_order_release);
        BOOST_ASSERT(failure_order != memory_order_acq_rel);
        BOOST_ASSERT(cas_failure_order_must_not_be_stronger_than_success_order(success_order, failure_order));

#if defined(BOOST_ATOMIC_DETAIL_STORAGE_TYPE_MAY_ALIAS)
        return operations::compare_exchange_strong(m_storage.value, reinterpret_cast< storage_type& >(expected), atomics::detail::bitwise_cast< storage_type >(desired), success_order, failure_order);
#else
        storage_type old_value = atomics::detail::bitwise_cast< storage_type >(expected);
        const bool res = operations::compare_exchange_strong(m_storage.value, old_value, atomics::detail::bitwise_cast< storage_type >(desired), success_order, failure_order);
        expected = atomics::detail::bitwise_cast< value_type >(old_value);
        return res;
#endif
    }

    BOOST_FORCEINLINE bool compare_exchange_strong(value_type& expected, value_type desired, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return compare_exchange_strong(expected, desired, order, atomics::detail::deduce_failure_order(order));
    }

    BOOST_FORCEINLINE bool compare_exchange_weak(value_type& expected, value_type desired, memory_order success_order, memory_order failure_order) volatile BOOST_NOEXCEPT
    {
        BOOST_ASSERT(failure_order != memory_order_release);
        BOOST_ASSERT(failure_order != memory_order_acq_rel);
        BOOST_ASSERT(cas_failure_order_must_not_be_stronger_than_success_order(success_order, failure_order));

#if defined(BOOST_ATOMIC_DETAIL_STORAGE_TYPE_MAY_ALIAS)
        return operations::compare_exchange_weak(m_storage.value, reinterpret_cast< storage_type& >(expected), atomics::detail::bitwise_cast< storage_type >(desired), success_order, failure_order);
#else
        storage_type old_value = atomics::detail::bitwise_cast< storage_type >(expected);
        const bool res = operations::compare_exchange_weak(m_storage.value, old_value, atomics::detail::bitwise_cast< storage_type >(desired), success_order, failure_order);
        expected = atomics::detail::bitwise_cast< value_type >(old_value);
        return res;
#endif
    }

    BOOST_FORCEINLINE bool compare_exchange_weak(value_type& expected, value_type desired, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return compare_exchange_weak(expected, desired, order, atomics::detail::deduce_failure_order(order));
    }

    // Boost.Atomic extensions
    BOOST_FORCEINLINE void opaque_add(difference_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        extra_operations::opaque_add(m_storage.value, static_cast< storage_type >(v * sizeof(T)), order);
    }

    BOOST_FORCEINLINE void opaque_sub(difference_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        extra_operations::opaque_sub(m_storage.value, static_cast< storage_type >(v * sizeof(T)), order);
    }

    BOOST_FORCEINLINE bool add_and_test(difference_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return extra_operations::add_and_test(m_storage.value, static_cast< storage_type >(v * sizeof(T)), order);
    }

    BOOST_FORCEINLINE bool sub_and_test(difference_type v, memory_order order = memory_order_seq_cst) volatile BOOST_NOEXCEPT
    {
        return extra_operations::sub_and_test(m_storage.value, static_cast< storage_type >(v * sizeof(T)), order);
    }

    // Operators
    BOOST_FORCEINLINE value_type operator++(int) volatile BOOST_NOEXCEPT
    {
        return fetch_add(1);
    }

    BOOST_FORCEINLINE value_type operator++() volatile BOOST_NOEXCEPT
    {
        return fetch_add(1) + 1;
    }

    BOOST_FORCEINLINE value_type operator--(int) volatile BOOST_NOEXCEPT
    {
        return fetch_sub(1);
    }

    BOOST_FORCEINLINE value_type operator--() volatile BOOST_NOEXCEPT
    {
        return fetch_sub(1) - 1;
    }

    BOOST_FORCEINLINE value_type operator+=(difference_type v) volatile BOOST_NOEXCEPT
    {
        return fetch_add(v) + v;
    }

    BOOST_FORCEINLINE value_type operator-=(difference_type v) volatile BOOST_NOEXCEPT
    {
        return fetch_sub(v) - v;
    }

    BOOST_DELETED_FUNCTION(base_atomic(base_atomic const&))
    BOOST_DELETED_FUNCTION(base_atomic& operator=(base_atomic const&))
};

} // namespace detail

template< typename T >
class atomic :
    public atomics::detail::base_atomic< T, typename atomics::detail::classify< T >::type >
{
private:
    typedef atomics::detail::base_atomic< T, typename atomics::detail::classify< T >::type > base_type;
    typedef typename base_type::value_arg_type value_arg_type;

public:
    typedef typename base_type::value_type value_type;
    typedef typename base_type::storage_type storage_type;

public:
    static BOOST_CONSTEXPR_OR_CONST bool is_always_lock_free = base_type::operations::is_always_lock_free;

public:
    BOOST_DEFAULTED_FUNCTION(atomic(), BOOST_NOEXCEPT {})

    // NOTE: The constructor is made explicit because gcc 4.7 complains that
    //       operator=(value_arg_type) is considered ambiguous with operator=(atomic const&)
    //       in assignment expressions, even though conversion to atomic<> is less preferred
    //       than conversion to value_arg_type.
    BOOST_FORCEINLINE explicit BOOST_CONSTEXPR atomic(value_arg_type v) BOOST_NOEXCEPT : base_type(v) {}

    BOOST_FORCEINLINE value_type operator= (value_arg_type v) volatile BOOST_NOEXCEPT
    {
        this->store(v);
        return v;
    }

    BOOST_FORCEINLINE operator value_type() const volatile BOOST_NOEXCEPT
    {
        return this->load();
    }

    BOOST_FORCEINLINE bool is_lock_free() const volatile BOOST_NOEXCEPT
    {
        // C++17 requires all instances of atomic<> return a value consistent with is_always_lock_free here
        return is_always_lock_free;
    }

    BOOST_FORCEINLINE storage_type& storage() BOOST_NOEXCEPT { return this->m_storage.value; }
    BOOST_FORCEINLINE storage_type volatile& storage() volatile BOOST_NOEXCEPT { return this->m_storage.value; }
    BOOST_FORCEINLINE storage_type const& storage() const BOOST_NOEXCEPT { return this->m_storage.value; }
    BOOST_FORCEINLINE storage_type const volatile& storage() const volatile BOOST_NOEXCEPT { return this->m_storage.value; }

    BOOST_DELETED_FUNCTION(atomic(atomic const&))
    BOOST_DELETED_FUNCTION(atomic& operator= (atomic const&))
    BOOST_DELETED_FUNCTION(atomic& operator= (atomic const&) volatile)
};

template< typename T >
BOOST_CONSTEXPR_OR_CONST bool atomic< T >::is_always_lock_free;

typedef atomic< char > atomic_char;
typedef atomic< unsigned char > atomic_uchar;
typedef atomic< signed char > atomic_schar;
typedef atomic< uint8_t > atomic_uint8_t;
typedef atomic< int8_t > atomic_int8_t;
typedef atomic< unsigned short > atomic_ushort;
typedef atomic< short > atomic_short;
typedef atomic< uint16_t > atomic_uint16_t;
typedef atomic< int16_t > atomic_int16_t;
typedef atomic< unsigned int > atomic_uint;
typedef atomic< int > atomic_int;
typedef atomic< uint32_t > atomic_uint32_t;
typedef atomic< int32_t > atomic_int32_t;
typedef atomic< unsigned long > atomic_ulong;
typedef atomic< long > atomic_long;
typedef atomic< uint64_t > atomic_uint64_t;
typedef atomic< int64_t > atomic_int64_t;
#ifdef BOOST_HAS_LONG_LONG
typedef atomic< boost::ulong_long_type > atomic_ullong;
typedef atomic< boost::long_long_type > atomic_llong;
#endif
typedef atomic< void* > atomic_address;
typedef atomic< bool > atomic_bool;
typedef atomic< wchar_t > atomic_wchar_t;
#if !defined(BOOST_NO_CXX11_CHAR16_T)
typedef atomic< char16_t > atomic_char16_t;
#endif
#if !defined(BOOST_NO_CXX11_CHAR32_T)
typedef atomic< char32_t > atomic_char32_t;
#endif

typedef atomic< int_least8_t > atomic_int_least8_t;
typedef atomic< uint_least8_t > atomic_uint_least8_t;
typedef atomic< int_least16_t > atomic_int_least16_t;
typedef atomic< uint_least16_t > atomic_uint_least16_t;
typedef atomic< int_least32_t > atomic_int_least32_t;
typedef atomic< uint_least32_t > atomic_uint_least32_t;
typedef atomic< int_least64_t > atomic_int_least64_t;
typedef atomic< uint_least64_t > atomic_uint_least64_t;
typedef atomic< int_fast8_t > atomic_int_fast8_t;
typedef atomic< uint_fast8_t > atomic_uint_fast8_t;
typedef atomic< int_fast16_t > atomic_int_fast16_t;
typedef atomic< uint_fast16_t > atomic_uint_fast16_t;
typedef atomic< int_fast32_t > atomic_int_fast32_t;
typedef atomic< uint_fast32_t > atomic_uint_fast32_t;
typedef atomic< int_fast64_t > atomic_int_fast64_t;
typedef atomic< uint_fast64_t > atomic_uint_fast64_t;
typedef atomic< intmax_t > atomic_intmax_t;
typedef atomic< uintmax_t > atomic_uintmax_t;

typedef atomic< std::size_t > atomic_size_t;
typedef atomic< std::ptrdiff_t > atomic_ptrdiff_t;

#if defined(BOOST_HAS_INTPTR_T)
typedef atomic< intptr_t > atomic_intptr_t;
typedef atomic< uintptr_t > atomic_uintptr_t;
#endif

} // namespace atomics
} // namespace boost

#if defined(BOOST_MSVC)
#pragma warning(pop)
#endif

#endif // BOOST_ATOMIC_DETAIL_ATOMIC_TEMPLATE_HPP_INCLUDED_
