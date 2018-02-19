/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2017 Andrey Semashev
 */
/*!
 * \file   atomic/detail/extra_ops_msvc_x86.hpp
 *
 * This header contains implementation of the extra atomic operations for x86.
 */

#ifndef BOOST_ATOMIC_DETAIL_EXTRA_OPS_MSVC_X86_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_EXTRA_OPS_MSVC_X86_HPP_INCLUDED_

#include <cstddef>
#include <boost/memory_order.hpp>
#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/interlocked.hpp>
#include <boost/atomic/detail/storage_type.hpp>
#include <boost/atomic/detail/extra_operations_fwd.hpp>
#include <boost/atomic/detail/extra_ops_generic.hpp>
#include <boost/atomic/capabilities.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

#if defined(BOOST_MSVC)
#pragma warning(push)
// frame pointer register 'ebx' modified by inline assembly code
#pragma warning(disable: 4731)
#endif

namespace boost {
namespace atomics {
namespace detail {

#if defined(_M_IX86) || (defined(BOOST_ATOMIC_INTERLOCKED_BTS) && defined(BOOST_ATOMIC_INTERLOCKED_BTR))

template< typename Base, std::size_t Size, bool Signed >
struct msvc_x86_extra_operations_common :
    public generic_extra_operations< Base, Size, Signed >
{
    typedef generic_extra_operations< Base, Size, Signed > base_type;
    typedef typename base_type::storage_type storage_type;

#if defined(BOOST_ATOMIC_INTERLOCKED_BTS)
    static BOOST_FORCEINLINE bool bit_test_and_set(storage_type volatile& storage, unsigned int bit_number, memory_order) BOOST_NOEXCEPT
    {
        return !!BOOST_ATOMIC_INTERLOCKED_BTS(&storage, bit_number);
    }
#else
    static BOOST_FORCEINLINE bool bit_test_and_set(storage_type volatile& storage, unsigned int bit_number, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        bool result;
        __asm
        {
            mov edx, storage
            mov eax, bit_number
            lock bts [edx], eax
            setc result
        };
        base_type::fence_after(order);
        return result;
    }
#endif

#if defined(BOOST_ATOMIC_INTERLOCKED_BTR)
    static BOOST_FORCEINLINE bool bit_test_and_reset(storage_type volatile& storage, unsigned int bit_number, memory_order) BOOST_NOEXCEPT
    {
        return !!BOOST_ATOMIC_INTERLOCKED_BTR(&storage, bit_number);
    }
#else
    static BOOST_FORCEINLINE bool bit_test_and_reset(storage_type volatile& storage, unsigned int bit_number, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        bool result;
        __asm
        {
            mov edx, storage
            mov eax, bit_number
            lock btr [edx], eax
            setc result
        };
        base_type::fence_after(order);
        return result;
    }
#endif

#if defined(_M_IX86)
    static BOOST_FORCEINLINE bool bit_test_and_complement(storage_type volatile& storage, unsigned int bit_number, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        bool result;
        __asm
        {
            mov edx, storage
            mov eax, bit_number
            lock btc [edx], eax
            setc result
        };
        base_type::fence_after(order);
        return result;
    }
#endif
};

template< typename Base, bool Signed >
struct extra_operations< Base, 1u, Signed > :
    public msvc_x86_extra_operations_common< Base, 1u, Signed >
{
    typedef msvc_x86_extra_operations_common< Base, 1u, Signed > base_type;
    typedef typename base_type::storage_type storage_type;

#if defined(_M_IX86)
    static BOOST_FORCEINLINE storage_type fetch_negate(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        storage_type old_val;
        __asm
        {
            mov ecx, storage
            movzx eax, byte ptr [ecx]
            align 16
        again:
            mov edx, eax
            neg dl
            lock cmpxchg byte ptr [ecx], dl
            jne again
            mov old_val, al
        };
        base_type::fence_after(order);
        return old_val;
    }

    static BOOST_FORCEINLINE void opaque_negate(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov ecx, storage
            movzx eax, byte ptr [ecx]
            align 16
        again:
            mov edx, eax
            neg dl
            lock cmpxchg byte ptr [ecx], dl
            jne again
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE storage_type fetch_complement(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        storage_type old_val;
        __asm
        {
            mov ecx, storage
            movzx eax, byte ptr [ecx]
            align 16
        again:
            mov edx, eax
            not dl
            lock cmpxchg byte ptr [ecx], dl
            jne again
            mov old_val, al
        };
        base_type::fence_after(order);
        return old_val;
    }

    static BOOST_FORCEINLINE void opaque_complement(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov ecx, storage
            movzx eax, byte ptr [ecx]
            align 16
        again:
            mov edx, eax
            not dl
            lock cmpxchg byte ptr [ecx], dl
            jne again
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE void opaque_add(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            movzx eax, v
            lock add byte ptr [edx], al
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE void opaque_sub(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            movzx eax, v
            lock sub byte ptr [edx], al
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE void opaque_negate(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            lock neg byte ptr [edx]
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE void opaque_and(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            movzx eax, v
            lock and byte ptr [edx], al
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE void opaque_or(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            movzx eax, v
            lock or byte ptr [edx], al
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE void opaque_xor(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            movzx eax, v
            lock xor byte ptr [edx], al
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE void opaque_complement(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            lock not byte ptr [edx]
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE bool add_and_test(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        bool result;
        __asm
        {
            mov edx, storage
            movzx eax, v
            lock add byte ptr [edx], al
            setz result
        };
        base_type::fence_after(order);
        return result;
    }

    static BOOST_FORCEINLINE bool sub_and_test(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        bool result;
        __asm
        {
            mov edx, storage
            movzx eax, v
            lock sub byte ptr [edx], al
            setz result
        };
        base_type::fence_after(order);
        return result;
    }

    static BOOST_FORCEINLINE bool and_and_test(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        bool result;
        __asm
        {
            mov edx, storage
            movzx eax, v
            lock and byte ptr [edx], al
            setz result
        };
        base_type::fence_after(order);
        return result;
    }

    static BOOST_FORCEINLINE bool or_and_test(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        bool result;
        __asm
        {
            mov edx, storage
            movzx eax, v
            lock or byte ptr [edx], al
            setz result
        };
        base_type::fence_after(order);
        return result;
    }

    static BOOST_FORCEINLINE bool xor_and_test(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        bool result;
        __asm
        {
            mov edx, storage
            movzx eax, v
            lock xor byte ptr [edx], al
            setz result
        };
        base_type::fence_after(order);
        return result;
    }
#endif // defined(_M_IX86)
};

template< typename Base, bool Signed >
struct extra_operations< Base, 2u, Signed > :
    public msvc_x86_extra_operations_common< Base, 2u, Signed >
{
    typedef msvc_x86_extra_operations_common< Base, 2u, Signed > base_type;
    typedef typename base_type::storage_type storage_type;

#if defined(_M_IX86)
    static BOOST_FORCEINLINE storage_type fetch_negate(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        storage_type old_val;
        __asm
        {
            mov ecx, storage
            movzx eax, word ptr [ecx]
            align 16
        again:
            mov edx, eax
            neg dx
            lock cmpxchg word ptr [ecx], dx
            jne again
            mov old_val, ax
        };
        base_type::fence_after(order);
        return old_val;
    }

    static BOOST_FORCEINLINE void opaque_negate(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov ecx, storage
            movzx eax, word ptr [ecx]
            align 16
        again:
            mov edx, eax
            neg dx
            lock cmpxchg word ptr [ecx], dx
            jne again
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE storage_type fetch_complement(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        storage_type old_val;
        __asm
        {
            mov ecx, storage
            movzx eax, word ptr [ecx]
            align 16
        again:
            mov edx, eax
            not dx
            lock cmpxchg word ptr [ecx], dx
            jne again
            mov old_val, ax
        };
        base_type::fence_after(order);
        return old_val;
    }

    static BOOST_FORCEINLINE void opaque_complement(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov ecx, storage
            movzx eax, word ptr [ecx]
            align 16
        again:
            mov edx, eax
            not dx
            lock cmpxchg word ptr [ecx], dx
            jne again
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE void opaque_add(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            movzx eax, v
            lock add word ptr [edx], ax
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE void opaque_sub(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            movzx eax, v
            lock sub word ptr [edx], ax
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE void opaque_negate(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            lock neg word ptr [edx]
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE void opaque_and(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            movzx eax, v
            lock and word ptr [edx], ax
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE void opaque_or(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            movzx eax, v
            lock or word ptr [edx], ax
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE void opaque_xor(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            movzx eax, v
            lock xor word ptr [edx], ax
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE void opaque_complement(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            lock not word ptr [edx]
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE bool add_and_test(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        bool result;
        __asm
        {
            mov edx, storage
            movzx eax, v
            lock add word ptr [edx], ax
            setz result
        };
        base_type::fence_after(order);
        return result;
    }

    static BOOST_FORCEINLINE bool sub_and_test(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        bool result;
        __asm
        {
            mov edx, storage
            movzx eax, v
            lock sub word ptr [edx], ax
            setz result
        };
        base_type::fence_after(order);
        return result;
    }

    static BOOST_FORCEINLINE bool and_and_test(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        bool result;
        __asm
        {
            mov edx, storage
            movzx eax, v
            lock and word ptr [edx], ax
            setz result
        };
        base_type::fence_after(order);
        return result;
    }

    static BOOST_FORCEINLINE bool or_and_test(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        bool result;
        __asm
        {
            mov edx, storage
            movzx eax, v
            lock or word ptr [edx], ax
            setz result
        };
        base_type::fence_after(order);
        return result;
    }

    static BOOST_FORCEINLINE bool xor_and_test(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        bool result;
        __asm
        {
            mov edx, storage
            movzx eax, v
            lock xor word ptr [edx], ax
            setz result
        };
        base_type::fence_after(order);
        return result;
    }
#endif // defined(_M_IX86)
};

template< typename Base, bool Signed >
struct extra_operations< Base, 4u, Signed > :
    public msvc_x86_extra_operations_common< Base, 4u, Signed >
{
    typedef msvc_x86_extra_operations_common< Base, 4u, Signed > base_type;
    typedef typename base_type::storage_type storage_type;

#if defined(_M_IX86)
    static BOOST_FORCEINLINE storage_type fetch_negate(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        storage_type old_val;
        __asm
        {
            mov ecx, storage
            mov eax, dword ptr [ecx]
            align 16
        again:
            mov edx, eax
            neg edx
            lock cmpxchg dword ptr [ecx], edx
            jne again
            mov old_val, eax
        };
        base_type::fence_after(order);
        return old_val;
    }

    static BOOST_FORCEINLINE void opaque_negate(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov ecx, storage
            mov eax, dword ptr [ecx]
            align 16
        again:
            mov edx, eax
            neg edx
            lock cmpxchg dword ptr [ecx], edx
            jne again
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE storage_type fetch_complement(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        storage_type old_val;
        __asm
        {
            mov ecx, storage
            mov eax, dword ptr [ecx]
            align 16
        again:
            mov edx, eax
            not edx
            lock cmpxchg dword ptr [ecx], edx
            jne again
            mov old_val, eax
        };
        base_type::fence_after(order);
        return old_val;
    }

    static BOOST_FORCEINLINE void opaque_complement(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov ecx, storage
            mov eax, dword ptr [ecx]
            align 16
        again:
            mov edx, eax
            not edx
            lock cmpxchg dword ptr [ecx], edx
            jne again
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE void opaque_add(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            mov eax, v
            lock add dword ptr [edx], eax
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE void opaque_sub(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            mov eax, v
            lock sub dword ptr [edx], eax
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE void opaque_negate(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            lock neg dword ptr [edx]
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE void opaque_and(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            mov eax, v
            lock and dword ptr [edx], eax
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE void opaque_or(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            mov eax, v
            lock or dword ptr [edx], eax
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE void opaque_xor(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            mov eax, v
            lock xor dword ptr [edx], eax
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE void opaque_complement(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            lock not dword ptr [edx]
        };
        base_type::fence_after(order);
    }

    static BOOST_FORCEINLINE bool add_and_test(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        bool result;
        __asm
        {
            mov edx, storage
            mov eax, v
            lock add dword ptr [edx], eax
            setz result
        };
        base_type::fence_after(order);
        return result;
    }

    static BOOST_FORCEINLINE bool sub_and_test(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        bool result;
        __asm
        {
            mov edx, storage
            mov eax, v
            lock sub dword ptr [edx], eax
            setz result
        };
        base_type::fence_after(order);
        return result;
    }

    static BOOST_FORCEINLINE bool and_and_test(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        bool result;
        __asm
        {
            mov edx, storage
            mov eax, v
            lock and dword ptr [edx], eax
            setz result
        };
        base_type::fence_after(order);
        return result;
    }

    static BOOST_FORCEINLINE bool or_and_test(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        bool result;
        __asm
        {
            mov edx, storage
            mov eax, v
            lock or dword ptr [edx], eax
            setz result
        };
        base_type::fence_after(order);
        return result;
    }

    static BOOST_FORCEINLINE bool xor_and_test(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        bool result;
        __asm
        {
            mov edx, storage
            mov eax, v
            lock xor dword ptr [edx], eax
            setz result
        };
        base_type::fence_after(order);
        return result;
    }
#endif // defined(_M_IX86)
};

#endif // defined(_M_IX86) || (defined(BOOST_ATOMIC_INTERLOCKED_BTS) && defined(BOOST_ATOMIC_INTERLOCKED_BTR))

#if defined(BOOST_ATOMIC_INTERLOCKED_BTS64) && defined(BOOST_ATOMIC_INTERLOCKED_BTR64)

template< typename Base, bool Signed >
struct extra_operations< Base, 8u, Signed > :
    public generic_extra_operations< Base, 8u, Signed >
{
    typedef generic_extra_operations< Base, 8u, Signed > base_type;
    typedef typename base_type::storage_type storage_type;

    static BOOST_FORCEINLINE bool bit_test_and_set(storage_type volatile& storage, unsigned int bit_number, memory_order order) BOOST_NOEXCEPT
    {
        return !!BOOST_ATOMIC_INTERLOCKED_BTS64(&storage, bit_number);
    }

    static BOOST_FORCEINLINE bool bit_test_and_reset(storage_type volatile& storage, unsigned int bit_number, memory_order order) BOOST_NOEXCEPT
    {
        return !!BOOST_ATOMIC_INTERLOCKED_BTR64(&storage, bit_number);
    }
};

#endif // defined(BOOST_ATOMIC_INTERLOCKED_BTS64) && defined(BOOST_ATOMIC_INTERLOCKED_BTR64)

} // namespace detail
} // namespace atomics
} // namespace boost

#if defined(BOOST_MSVC)
#pragma warning(pop)
#endif

#endif // BOOST_ATOMIC_DETAIL_EXTRA_OPS_MSVC_X86_HPP_INCLUDED_
