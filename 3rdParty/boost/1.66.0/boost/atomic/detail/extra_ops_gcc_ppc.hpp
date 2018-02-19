/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2017 Andrey Semashev
 */
/*!
 * \file   atomic/detail/extra_ops_gcc_ppc.hpp
 *
 * This header contains implementation of the extra atomic operations for PowerPC.
 */

#ifndef BOOST_ATOMIC_DETAIL_EXTRA_OPS_GCC_PPC_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_EXTRA_OPS_GCC_PPC_HPP_INCLUDED_

#include <cstddef>
#include <boost/memory_order.hpp>
#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/storage_type.hpp>
#include <boost/atomic/detail/extra_operations_fwd.hpp>
#include <boost/atomic/detail/extra_ops_generic.hpp>
#include <boost/atomic/detail/ops_gcc_ppc_common.hpp>
#include <boost/atomic/capabilities.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {
namespace atomics {
namespace detail {

#if defined(BOOST_ATOMIC_DETAIL_PPC_HAS_LBARX_STBCX)

template< typename Base, bool Signed >
struct extra_operations< Base, 1u, Signed > :
    public generic_extra_operations< Base, 1u, Signed >
{
    typedef generic_extra_operations< Base, 1u, Signed > base_type;
    typedef typename base_type::storage_type storage_type;

    static BOOST_FORCEINLINE storage_type fetch_negate(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        gcc_ppc_operations_base::fence_before(order);
        storage_type original, tmp;
        __asm__ __volatile__
        (
            "1:\n\t"
            "lbarx %0,%y2\n\t"
            "neg %1,%0\n\t"
            "stbcx. %1,%y2\n\t"
            "bne- 1b\n\t"
            : "=&b" (original), "=&b" (tmp), "+Z" (storage)
            :
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC
        );
        gcc_ppc_operations_base::fence_after(order);
        return original;
    }

    static BOOST_FORCEINLINE storage_type fetch_complement(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        gcc_ppc_operations_base::fence_before(order);
        storage_type original, tmp;
        __asm__ __volatile__
        (
            "1:\n\t"
            "lbarx %0,%y2\n\t"
            "nor %1,%0,%0\n\t"
            "stbcx. %1,%y2\n\t"
            "bne- 1b\n\t"
            : "=&b" (original), "=&b" (tmp), "+Z" (storage)
            :
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC
        );
        gcc_ppc_operations_base::fence_after(order);
        return original;
    }

    static BOOST_FORCEINLINE void opaque_negate(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        fetch_negate(storage, order);
    }

    static BOOST_FORCEINLINE void opaque_complement(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        fetch_complement(storage, order);
    }
};

#endif // defined(BOOST_ATOMIC_DETAIL_PPC_HAS_LBARX_STBCX)

#if defined(BOOST_ATOMIC_DETAIL_PPC_HAS_LHARX_STHCX)

template< typename Base, bool Signed >
struct extra_operations< Base, 2u, Signed > :
    public generic_extra_operations< Base, 2u, Signed >
{
    typedef generic_extra_operations< Base, 2u, Signed > base_type;
    typedef typename base_type::storage_type storage_type;

    static BOOST_FORCEINLINE storage_type fetch_negate(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        gcc_ppc_operations_base::fence_before(order);
        storage_type original, tmp;
        __asm__ __volatile__
        (
            "1:\n\t"
            "lharx %0,%y2\n\t"
            "neg %1,%0\n\t"
            "sthcx. %1,%y2\n\t"
            "bne- 1b\n\t"
            : "=&b" (original), "=&b" (tmp), "+Z" (storage)
            :
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC
        );
        gcc_ppc_operations_base::fence_after(order);
        return original;
    }

    static BOOST_FORCEINLINE storage_type fetch_complement(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        gcc_ppc_operations_base::fence_before(order);
        storage_type original, tmp;
        __asm__ __volatile__
        (
            "1:\n\t"
            "lharx %0,%y2\n\t"
            "nor %1,%0,%0\n\t"
            "sthcx. %1,%y2\n\t"
            "bne- 1b\n\t"
            : "=&b" (original), "=&b" (tmp), "+Z" (storage)
            :
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC
        );
        gcc_ppc_operations_base::fence_after(order);
        return original;
    }

    static BOOST_FORCEINLINE void opaque_negate(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        fetch_negate(storage, order);
    }

    static BOOST_FORCEINLINE void opaque_complement(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        fetch_complement(storage, order);
    }
};

#endif // defined(BOOST_ATOMIC_DETAIL_PPC_HAS_LHARX_STHCX)

template< typename Base, bool Signed >
struct extra_operations< Base, 4u, Signed > :
    public generic_extra_operations< Base, 4u, Signed >
{
    typedef generic_extra_operations< Base, 4u, Signed > base_type;
    typedef typename base_type::storage_type storage_type;

    static BOOST_FORCEINLINE storage_type fetch_negate(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        gcc_ppc_operations_base::fence_before(order);
        storage_type original, tmp;
        __asm__ __volatile__
        (
            "1:\n\t"
            "lwarx %0,%y2\n\t"
            "neg %1,%0\n\t"
            "stwcx. %1,%y2\n\t"
            "bne- 1b\n\t"
            : "=&b" (original), "=&b" (tmp), "+Z" (storage)
            :
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC
        );
        gcc_ppc_operations_base::fence_after(order);
        return original;
    }

    static BOOST_FORCEINLINE storage_type fetch_complement(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        gcc_ppc_operations_base::fence_before(order);
        storage_type original, tmp;
        __asm__ __volatile__
        (
            "1:\n\t"
            "lwarx %0,%y2\n\t"
            "nor %1,%0,%0\n\t"
            "stwcx. %1,%y2\n\t"
            "bne- 1b\n\t"
            : "=&b" (original), "=&b" (tmp), "+Z" (storage)
            :
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC
        );
        gcc_ppc_operations_base::fence_after(order);
        return original;
    }

    static BOOST_FORCEINLINE void opaque_negate(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        fetch_negate(storage, order);
    }

    static BOOST_FORCEINLINE void opaque_complement(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        fetch_complement(storage, order);
    }
};

#if defined(BOOST_ATOMIC_DETAIL_PPC_HAS_LDARX_STDCX)

template< typename Base, bool Signed >
struct extra_operations< Base, 8u, Signed > :
    public generic_extra_operations< Base, 8u, Signed >
{
    typedef generic_extra_operations< Base, 8u, Signed > base_type;
    typedef typename base_type::storage_type storage_type;

    static BOOST_FORCEINLINE storage_type fetch_negate(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        gcc_ppc_operations_base::fence_before(order);
        storage_type original, tmp;
        __asm__ __volatile__
        (
            "1:\n\t"
            "ldarx %0,%y2\n\t"
            "neg %1,%0\n\t"
            "stdcx. %1,%y2\n\t"
            "bne- 1b\n\t"
            : "=&b" (original), "=&b" (tmp), "+Z" (storage)
            :
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC
        );
        gcc_ppc_operations_base::fence_after(order);
        return original;
    }

    static BOOST_FORCEINLINE storage_type fetch_complement(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        gcc_ppc_operations_base::fence_before(order);
        storage_type original, tmp;
        __asm__ __volatile__
        (
            "1:\n\t"
            "ldarx %0,%y2\n\t"
            "nor %1,%0,%0\n\t"
            "stdcx. %1,%y2\n\t"
            "bne- 1b\n\t"
            : "=&b" (original), "=&b" (tmp), "+Z" (storage)
            :
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC
        );
        gcc_ppc_operations_base::fence_after(order);
        return original;
    }

    static BOOST_FORCEINLINE void opaque_negate(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        fetch_negate(storage, order);
    }

    static BOOST_FORCEINLINE void opaque_complement(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        fetch_complement(storage, order);
    }
};

#endif // defined(BOOST_ATOMIC_DETAIL_PPC_HAS_LDARX_STDCX)

} // namespace detail
} // namespace atomics
} // namespace boost

#endif // BOOST_ATOMIC_DETAIL_EXTRA_OPS_GCC_ARM_PPC_INCLUDED_
