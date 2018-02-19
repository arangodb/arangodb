/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2009 Helge Bahmann
 * Copyright (c) 2012 Tim Blechmann
 * Copyright (c) 2014 Andrey Semashev
 */
/*!
 * \file   atomic/detail/ops_msvc_x86.hpp
 *
 * This header contains implementation of the \c operations template.
 */

#ifndef BOOST_ATOMIC_DETAIL_OPS_MSVC_X86_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_OPS_MSVC_X86_HPP_INCLUDED_

#include <cstddef>
#include <boost/memory_order.hpp>
#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/interlocked.hpp>
#include <boost/atomic/detail/storage_type.hpp>
#include <boost/atomic/detail/operations_fwd.hpp>
#include <boost/atomic/detail/type_traits/make_signed.hpp>
#include <boost/atomic/capabilities.hpp>
#if defined(BOOST_ATOMIC_DETAIL_X86_HAS_CMPXCHG8B) || defined(BOOST_ATOMIC_DETAIL_X86_HAS_CMPXCHG16B)
#include <boost/cstdint.hpp>
#include <boost/atomic/detail/ops_cas_based.hpp>
#endif
#include <boost/atomic/detail/ops_msvc_common.hpp>
#if !defined(_M_IX86) && !(defined(BOOST_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE8) && defined(BOOST_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE16))
#include <boost/atomic/detail/ops_extending_cas_based.hpp>
#endif

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

#if defined(BOOST_MSVC)
#pragma warning(push)
// frame pointer register 'ebx' modified by inline assembly code. See the note below.
#pragma warning(disable: 4731)
#endif

#if defined(BOOST_ATOMIC_DETAIL_X86_HAS_MFENCE)
extern "C" void _mm_mfence(void);
#if defined(BOOST_MSVC)
#pragma intrinsic(_mm_mfence)
#endif
#endif

namespace boost {
namespace atomics {
namespace detail {

/*
 * Implementation note for asm blocks.
 *
 * http://msdn.microsoft.com/en-us/data/k1a8ss06%28v=vs.105%29
 *
 * Some SSE types require eight-byte stack alignment, forcing the compiler to emit dynamic stack-alignment code.
 * To be able to access both the local variables and the function parameters after the alignment, the compiler
 * maintains two frame pointers. If the compiler performs frame pointer omission (FPO), it will use EBP and ESP.
 * If the compiler does not perform FPO, it will use EBX and EBP. To ensure code runs correctly, do not modify EBX
 * in asm code if the function requires dynamic stack alignment as it could modify the frame pointer.
 * Either move the eight-byte aligned types out of the function, or avoid using EBX.
 *
 * Since we have no way of knowing that the compiler uses FPO, we have to always save and restore ebx
 * whenever we have to clobber it. Additionally, we disable warning C4731 above so that the compiler
 * doesn't spam about ebx use.
 */

struct msvc_x86_operations_base
{
    static BOOST_CONSTEXPR_OR_CONST bool is_always_lock_free = true;

    static BOOST_FORCEINLINE void hardware_full_fence() BOOST_NOEXCEPT
    {
#if defined(BOOST_ATOMIC_DETAIL_X86_HAS_MFENCE)
        _mm_mfence();
#else
        long tmp;
        BOOST_ATOMIC_INTERLOCKED_EXCHANGE(&tmp, 0);
#endif
    }

    static BOOST_FORCEINLINE void fence_before(memory_order) BOOST_NOEXCEPT
    {
        BOOST_ATOMIC_DETAIL_COMPILER_BARRIER();
    }

    static BOOST_FORCEINLINE void fence_after(memory_order) BOOST_NOEXCEPT
    {
        BOOST_ATOMIC_DETAIL_COMPILER_BARRIER();
    }

    static BOOST_FORCEINLINE void fence_after_load(memory_order) BOOST_NOEXCEPT
    {
        BOOST_ATOMIC_DETAIL_COMPILER_BARRIER();

        // On x86 and x86_64 there is no need for a hardware barrier,
        // even if seq_cst memory order is requested, because all
        // seq_cst writes are implemented with lock-prefixed operations
        // or xchg which has implied lock prefix. Therefore normal loads
        // are already ordered with seq_cst stores on these architectures.
    }
};

template< typename T, typename Derived >
struct msvc_x86_operations :
    public msvc_x86_operations_base
{
    typedef T storage_type;

    static BOOST_FORCEINLINE void store(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        if (order != memory_order_seq_cst)
        {
            fence_before(order);
            storage = v;
            fence_after(order);
        }
        else
        {
            Derived::exchange(storage, v, order);
        }
    }

    static BOOST_FORCEINLINE storage_type load(storage_type const volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        storage_type v = storage;
        fence_after_load(order);
        return v;
    }

    static BOOST_FORCEINLINE storage_type fetch_sub(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        typedef typename boost::atomics::detail::make_signed< storage_type >::type signed_storage_type;
        return Derived::fetch_add(storage, static_cast< storage_type >(-static_cast< signed_storage_type >(v)), order);
    }

    static BOOST_FORCEINLINE bool compare_exchange_weak(
        storage_type volatile& storage, storage_type& expected, storage_type desired, memory_order success_order, memory_order failure_order) BOOST_NOEXCEPT
    {
        return Derived::compare_exchange_strong(storage, expected, desired, success_order, failure_order);
    }

    static BOOST_FORCEINLINE bool test_and_set(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        return !!Derived::exchange(storage, (storage_type)1, order);
    }

    static BOOST_FORCEINLINE void clear(storage_type volatile& storage, memory_order order) BOOST_NOEXCEPT
    {
        store(storage, (storage_type)0, order);
    }
};

template< bool Signed >
struct operations< 4u, Signed > :
    public msvc_x86_operations< typename make_storage_type< 4u, Signed >::type, operations< 4u, Signed > >
{
    typedef msvc_x86_operations< typename make_storage_type< 4u, Signed >::type, operations< 4u, Signed > > base_type;
    typedef typename base_type::storage_type storage_type;
    typedef typename make_storage_type< 4u, Signed >::aligned aligned_storage_type;

    static BOOST_CONSTEXPR_OR_CONST std::size_t storage_size = 4u;
    static BOOST_CONSTEXPR_OR_CONST bool is_signed = Signed;

    static BOOST_FORCEINLINE storage_type fetch_add(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        return static_cast< storage_type >(BOOST_ATOMIC_INTERLOCKED_EXCHANGE_ADD(&storage, v));
    }

    static BOOST_FORCEINLINE storage_type exchange(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        return static_cast< storage_type >(BOOST_ATOMIC_INTERLOCKED_EXCHANGE(&storage, v));
    }

    static BOOST_FORCEINLINE bool compare_exchange_strong(
        storage_type volatile& storage, storage_type& expected, storage_type desired, memory_order, memory_order) BOOST_NOEXCEPT
    {
        storage_type previous = expected;
        storage_type old_val = static_cast< storage_type >(BOOST_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE(&storage, desired, previous));
        expected = old_val;
        return (previous == old_val);
    }

#if defined(BOOST_ATOMIC_INTERLOCKED_AND)
    static BOOST_FORCEINLINE storage_type fetch_and(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        return static_cast< storage_type >(BOOST_ATOMIC_INTERLOCKED_AND(&storage, v));
    }
#else
    static BOOST_FORCEINLINE storage_type fetch_and(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        storage_type res = storage;
        while (!compare_exchange_strong(storage, res, res & v, order, memory_order_relaxed)) {}
        return res;
    }
#endif

#if defined(BOOST_ATOMIC_INTERLOCKED_OR)
    static BOOST_FORCEINLINE storage_type fetch_or(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        return static_cast< storage_type >(BOOST_ATOMIC_INTERLOCKED_OR(&storage, v));
    }
#else
    static BOOST_FORCEINLINE storage_type fetch_or(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        storage_type res = storage;
        while (!compare_exchange_strong(storage, res, res | v, order, memory_order_relaxed)) {}
        return res;
    }
#endif

#if defined(BOOST_ATOMIC_INTERLOCKED_XOR)
    static BOOST_FORCEINLINE storage_type fetch_xor(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        return static_cast< storage_type >(BOOST_ATOMIC_INTERLOCKED_XOR(&storage, v));
    }
#else
    static BOOST_FORCEINLINE storage_type fetch_xor(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        storage_type res = storage;
        while (!compare_exchange_strong(storage, res, res ^ v, order, memory_order_relaxed)) {}
        return res;
    }
#endif
};

#if defined(BOOST_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE8)

template< bool Signed >
struct operations< 1u, Signed > :
    public msvc_x86_operations< typename make_storage_type< 1u, Signed >::type, operations< 1u, Signed > >
{
    typedef msvc_x86_operations< typename make_storage_type< 1u, Signed >::type, operations< 1u, Signed > > base_type;
    typedef typename base_type::storage_type storage_type;
    typedef typename make_storage_type< 1u, Signed >::aligned aligned_storage_type;

    static BOOST_CONSTEXPR_OR_CONST std::size_t storage_size = 1u;
    static BOOST_CONSTEXPR_OR_CONST bool is_signed = Signed;

    static BOOST_FORCEINLINE storage_type fetch_add(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        return static_cast< storage_type >(BOOST_ATOMIC_INTERLOCKED_EXCHANGE_ADD8(&storage, v));
    }

    static BOOST_FORCEINLINE storage_type exchange(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        return static_cast< storage_type >(BOOST_ATOMIC_INTERLOCKED_EXCHANGE8(&storage, v));
    }

    static BOOST_FORCEINLINE bool compare_exchange_strong(
        storage_type volatile& storage, storage_type& expected, storage_type desired, memory_order, memory_order) BOOST_NOEXCEPT
    {
        storage_type previous = expected;
        storage_type old_val = static_cast< storage_type >(BOOST_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE8(&storage, desired, previous));
        expected = old_val;
        return (previous == old_val);
    }

    static BOOST_FORCEINLINE storage_type fetch_and(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        return static_cast< storage_type >(BOOST_ATOMIC_INTERLOCKED_AND8(&storage, v));
    }

    static BOOST_FORCEINLINE storage_type fetch_or(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        return static_cast< storage_type >(BOOST_ATOMIC_INTERLOCKED_OR8(&storage, v));
    }

    static BOOST_FORCEINLINE storage_type fetch_xor(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        return static_cast< storage_type >(BOOST_ATOMIC_INTERLOCKED_XOR8(&storage, v));
    }
};

#elif defined(_M_IX86)

template< bool Signed >
struct operations< 1u, Signed > :
    public msvc_x86_operations< typename make_storage_type< 1u, Signed >::type, operations< 1u, Signed > >
{
    typedef msvc_x86_operations< typename make_storage_type< 1u, Signed >::type, operations< 1u, Signed > > base_type;
    typedef typename base_type::storage_type storage_type;
    typedef typename make_storage_type< 1u, Signed >::aligned aligned_storage_type;

    static BOOST_CONSTEXPR_OR_CONST std::size_t storage_size = 1u;
    static BOOST_CONSTEXPR_OR_CONST bool is_signed = Signed;

    static BOOST_FORCEINLINE storage_type fetch_add(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            movzx eax, v
            lock xadd byte ptr [edx], al
            mov v, al
        };
        base_type::fence_after(order);
        return v;
    }

    static BOOST_FORCEINLINE storage_type exchange(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            movzx eax, v
            xchg byte ptr [edx], al
            mov v, al
        };
        base_type::fence_after(order);
        return v;
    }

    static BOOST_FORCEINLINE bool compare_exchange_strong(
        storage_type volatile& storage, storage_type& expected, storage_type desired, memory_order success_order, memory_order) BOOST_NOEXCEPT
    {
        base_type::fence_before(success_order);
        bool success;
        __asm
        {
            mov esi, expected
            mov edi, storage
            movzx eax, byte ptr [esi]
            movzx edx, desired
            lock cmpxchg byte ptr [edi], dl
            mov byte ptr [esi], al
            sete success
        };
        // The success and failure fences are equivalent anyway
        base_type::fence_after(success_order);
        return success;
    }

    static BOOST_FORCEINLINE storage_type fetch_and(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        int backup;
        __asm
        {
            mov backup, ebx
            xor edx, edx
            mov edi, storage
            movzx ebx, v
            movzx eax, byte ptr [edi]
            align 16
        again:
            mov dl, al
            and dl, bl
            lock cmpxchg byte ptr [edi], dl
            jne again
            mov v, al
            mov ebx, backup
        };
        base_type::fence_after(order);
        return v;
    }

    static BOOST_FORCEINLINE storage_type fetch_or(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        int backup;
        __asm
        {
            mov backup, ebx
            xor edx, edx
            mov edi, storage
            movzx ebx, v
            movzx eax, byte ptr [edi]
            align 16
        again:
            mov dl, al
            or dl, bl
            lock cmpxchg byte ptr [edi], dl
            jne again
            mov v, al
            mov ebx, backup
        };
        base_type::fence_after(order);
        return v;
    }

    static BOOST_FORCEINLINE storage_type fetch_xor(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        int backup;
        __asm
        {
            mov backup, ebx
            xor edx, edx
            mov edi, storage
            movzx ebx, v
            movzx eax, byte ptr [edi]
            align 16
        again:
            mov dl, al
            xor dl, bl
            lock cmpxchg byte ptr [edi], dl
            jne again
            mov v, al
            mov ebx, backup
        };
        base_type::fence_after(order);
        return v;
    }
};

#else

template< bool Signed >
struct operations< 1u, Signed > :
    public extending_cas_based_operations< operations< 4u, Signed >, 1u, Signed >
{
};

#endif

#if defined(BOOST_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE16)

template< bool Signed >
struct operations< 2u, Signed > :
    public msvc_x86_operations< typename make_storage_type< 2u, Signed >::type, operations< 2u, Signed > >
{
    typedef msvc_x86_operations< typename make_storage_type< 2u, Signed >::type, operations< 2u, Signed > > base_type;
    typedef typename base_type::storage_type storage_type;
    typedef typename make_storage_type< 2u, Signed >::aligned aligned_storage_type;

    static BOOST_CONSTEXPR_OR_CONST std::size_t storage_size = 2u;
    static BOOST_CONSTEXPR_OR_CONST bool is_signed = Signed;

    static BOOST_FORCEINLINE storage_type fetch_add(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        return static_cast< storage_type >(BOOST_ATOMIC_INTERLOCKED_EXCHANGE_ADD16(&storage, v));
    }

    static BOOST_FORCEINLINE storage_type exchange(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        return static_cast< storage_type >(BOOST_ATOMIC_INTERLOCKED_EXCHANGE16(&storage, v));
    }

    static BOOST_FORCEINLINE bool compare_exchange_strong(
        storage_type volatile& storage, storage_type& expected, storage_type desired, memory_order, memory_order) BOOST_NOEXCEPT
    {
        storage_type previous = expected;
        storage_type old_val = static_cast< storage_type >(BOOST_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE16(&storage, desired, previous));
        expected = old_val;
        return (previous == old_val);
    }

    static BOOST_FORCEINLINE storage_type fetch_and(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        return static_cast< storage_type >(BOOST_ATOMIC_INTERLOCKED_AND16(&storage, v));
    }

    static BOOST_FORCEINLINE storage_type fetch_or(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        return static_cast< storage_type >(BOOST_ATOMIC_INTERLOCKED_OR16(&storage, v));
    }

    static BOOST_FORCEINLINE storage_type fetch_xor(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        return static_cast< storage_type >(BOOST_ATOMIC_INTERLOCKED_XOR16(&storage, v));
    }
};

#elif defined(_M_IX86)

template< bool Signed >
struct operations< 2u, Signed > :
    public msvc_x86_operations< typename make_storage_type< 2u, Signed >::type, operations< 2u, Signed > >
{
    typedef msvc_x86_operations< typename make_storage_type< 2u, Signed >::type, operations< 2u, Signed > > base_type;
    typedef typename base_type::storage_type storage_type;
    typedef typename make_storage_type< 2u, Signed >::aligned aligned_storage_type;

    static BOOST_CONSTEXPR_OR_CONST std::size_t storage_size = 2u;
    static BOOST_CONSTEXPR_OR_CONST bool is_signed = Signed;

    static BOOST_FORCEINLINE storage_type fetch_add(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            movzx eax, v
            lock xadd word ptr [edx], ax
            mov v, ax
        };
        base_type::fence_after(order);
        return v;
    }

    static BOOST_FORCEINLINE storage_type exchange(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        __asm
        {
            mov edx, storage
            movzx eax, v
            xchg word ptr [edx], ax
            mov v, ax
        };
        base_type::fence_after(order);
        return v;
    }

    static BOOST_FORCEINLINE bool compare_exchange_strong(
        storage_type volatile& storage, storage_type& expected, storage_type desired, memory_order success_order, memory_order) BOOST_NOEXCEPT
    {
        base_type::fence_before(success_order);
        bool success;
        __asm
        {
            mov esi, expected
            mov edi, storage
            movzx eax, word ptr [esi]
            movzx edx, desired
            lock cmpxchg word ptr [edi], dx
            mov word ptr [esi], ax
            sete success
        };
        // The success and failure fences are equivalent anyway
        base_type::fence_after(success_order);
        return success;
    }

    static BOOST_FORCEINLINE storage_type fetch_and(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        int backup;
        __asm
        {
            mov backup, ebx
            xor edx, edx
            mov edi, storage
            movzx ebx, v
            movzx eax, word ptr [edi]
            align 16
        again:
            mov dx, ax
            and dx, bx
            lock cmpxchg word ptr [edi], dx
            jne again
            mov v, ax
            mov ebx, backup
        };
        base_type::fence_after(order);
        return v;
    }

    static BOOST_FORCEINLINE storage_type fetch_or(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        int backup;
        __asm
        {
            mov backup, ebx
            xor edx, edx
            mov edi, storage
            movzx ebx, v
            movzx eax, word ptr [edi]
            align 16
        again:
            mov dx, ax
            or dx, bx
            lock cmpxchg word ptr [edi], dx
            jne again
            mov v, ax
            mov ebx, backup
        };
        base_type::fence_after(order);
        return v;
    }

    static BOOST_FORCEINLINE storage_type fetch_xor(storage_type volatile& storage, storage_type v, memory_order order) BOOST_NOEXCEPT
    {
        base_type::fence_before(order);
        int backup;
        __asm
        {
            mov backup, ebx
            xor edx, edx
            mov edi, storage
            movzx ebx, v
            movzx eax, word ptr [edi]
            align 16
        again:
            mov dx, ax
            xor dx, bx
            lock cmpxchg word ptr [edi], dx
            jne again
            mov v, ax
            mov ebx, backup
        };
        base_type::fence_after(order);
        return v;
    }
};

#else

template< bool Signed >
struct operations< 2u, Signed > :
    public extending_cas_based_operations< operations< 4u, Signed >, 2u, Signed >
{
};

#endif


#if defined(BOOST_ATOMIC_DETAIL_X86_HAS_CMPXCHG8B)

template< bool Signed >
struct msvc_dcas_x86
{
    typedef typename make_storage_type< 8u, Signed >::type storage_type;
    typedef typename make_storage_type< 8u, Signed >::aligned aligned_storage_type;

    static BOOST_CONSTEXPR_OR_CONST bool is_always_lock_free = true;

    static BOOST_CONSTEXPR_OR_CONST std::size_t storage_size = 8u;
    static BOOST_CONSTEXPR_OR_CONST bool is_signed = Signed;

    // Intel 64 and IA-32 Architectures Software Developer's Manual, Volume 3A, 8.1.1. Guaranteed Atomic Operations:
    //
    // The Pentium processor (and newer processors since) guarantees that the following additional memory operations will always be carried out atomically:
    // * Reading or writing a quadword aligned on a 64-bit boundary
    //
    // Luckily, the memory is almost always 8-byte aligned in our case because atomic<> uses 64 bit native types for storage and dynamic memory allocations
    // have at least 8 byte alignment. The only unfortunate case is when atomic is placed on the stack and it is not 8-byte aligned (like on 32 bit Windows).

    static BOOST_FORCEINLINE void store(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        BOOST_ATOMIC_DETAIL_COMPILER_BARRIER();

        storage_type volatile* p = &storage;
        if (((uint32_t)p & 0x00000007) == 0)
        {
#if defined(_M_IX86_FP) && _M_IX86_FP >= 2
#if defined(__AVX__)
            __asm
            {
                mov edx, p
                vmovq xmm4, v
                vmovq qword ptr [edx], xmm4
            };
#else
            __asm
            {
                mov edx, p
                movq xmm4, v
                movq qword ptr [edx], xmm4
            };
#endif
#else
            __asm
            {
                mov edx, p
                fild v
                fistp qword ptr [edx]
            };
#endif
        }
        else
        {
            int backup;
            __asm
            {
                mov backup, ebx
                mov edi, p
                mov ebx, dword ptr [v]
                mov ecx, dword ptr [v + 4]
                mov eax, dword ptr [edi]
                mov edx, dword ptr [edi + 4]
                align 16
            again:
                lock cmpxchg8b qword ptr [edi]
                jne again
                mov ebx, backup
            };
        }

        BOOST_ATOMIC_DETAIL_COMPILER_BARRIER();
    }

    static BOOST_FORCEINLINE storage_type load(storage_type const volatile& storage, memory_order) BOOST_NOEXCEPT
    {
        BOOST_ATOMIC_DETAIL_COMPILER_BARRIER();

        storage_type const volatile* p = &storage;
        storage_type value;

        if (((uint32_t)p & 0x00000007) == 0)
        {
#if defined(_M_IX86_FP) && _M_IX86_FP >= 2
#if defined(__AVX__)
            __asm
            {
                mov edx, p
                vmovq xmm4, qword ptr [edx]
                vmovq value, xmm4
            };
#else
            __asm
            {
                mov edx, p
                movq xmm4, qword ptr [edx]
                movq value, xmm4
            };
#endif
#else
            __asm
            {
                mov edx, p
                fild qword ptr [edx]
                fistp value
            };
#endif
        }
        else
        {
            // We don't care for comparison result here; the previous value will be stored into value anyway.
            // Also we don't care for ebx and ecx values, they just have to be equal to eax and edx before cmpxchg8b.
            __asm
            {
                mov edi, p
                mov eax, ebx
                mov edx, ecx
                lock cmpxchg8b qword ptr [edi]
                mov dword ptr [value], eax
                mov dword ptr [value + 4], edx
            };
        }

        BOOST_ATOMIC_DETAIL_COMPILER_BARRIER();

        return value;
    }

    static BOOST_FORCEINLINE bool compare_exchange_strong(
        storage_type volatile& storage, storage_type& expected, storage_type desired, memory_order, memory_order) BOOST_NOEXCEPT
    {
        // MSVC-11 in 32-bit mode sometimes generates messed up code without compiler barriers,
        // even though the _InterlockedCompareExchange64 intrinsic already provides one.
        BOOST_ATOMIC_DETAIL_COMPILER_BARRIER();

        storage_type volatile* p = &storage;
#if defined(BOOST_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE64)
        const storage_type old_val = (storage_type)BOOST_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE64(p, desired, expected);
        const bool result = (old_val == expected);
        expected = old_val;
#else
        bool result;
        int backup;
        __asm
        {
            mov backup, ebx
            mov edi, p
            mov esi, expected
            mov ebx, dword ptr [desired]
            mov ecx, dword ptr [desired + 4]
            mov eax, dword ptr [esi]
            mov edx, dword ptr [esi + 4]
            lock cmpxchg8b qword ptr [edi]
            mov dword ptr [esi], eax
            mov dword ptr [esi + 4], edx
            mov ebx, backup
            sete result
        };
#endif
        BOOST_ATOMIC_DETAIL_COMPILER_BARRIER();

        return result;
    }

    static BOOST_FORCEINLINE bool compare_exchange_weak(
        storage_type volatile& storage, storage_type& expected, storage_type desired, memory_order success_order, memory_order failure_order) BOOST_NOEXCEPT
    {
        return compare_exchange_strong(storage, expected, desired, success_order, failure_order);
    }

    static BOOST_FORCEINLINE storage_type exchange(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        BOOST_ATOMIC_DETAIL_COMPILER_BARRIER();

        storage_type volatile* p = &storage;
        int backup;
        __asm
        {
            mov backup, ebx
            mov edi, p
            mov ebx, dword ptr [v]
            mov ecx, dword ptr [v + 4]
            mov eax, dword ptr [edi]
            mov edx, dword ptr [edi + 4]
            align 16
        again:
            lock cmpxchg8b qword ptr [edi]
            jne again
            mov ebx, backup
            mov dword ptr [v], eax
            mov dword ptr [v + 4], edx
        };

        BOOST_ATOMIC_DETAIL_COMPILER_BARRIER();

        return v;
    }
};

template< bool Signed >
struct operations< 8u, Signed > :
    public cas_based_operations< msvc_dcas_x86< Signed > >
{
};

#elif defined(_M_AMD64)

template< bool Signed >
struct operations< 8u, Signed > :
    public msvc_x86_operations< typename make_storage_type< 8u, Signed >::type, operations< 8u, Signed > >
{
    typedef msvc_x86_operations< typename make_storage_type< 8u, Signed >::type, operations< 8u, Signed > > base_type;
    typedef typename base_type::storage_type storage_type;
    typedef typename make_storage_type< 8u, Signed >::aligned aligned_storage_type;

    static BOOST_CONSTEXPR_OR_CONST std::size_t storage_size = 8u;
    static BOOST_CONSTEXPR_OR_CONST bool is_signed = Signed;

    static BOOST_FORCEINLINE storage_type fetch_add(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        return static_cast< storage_type >(BOOST_ATOMIC_INTERLOCKED_EXCHANGE_ADD64(&storage, v));
    }

    static BOOST_FORCEINLINE storage_type exchange(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        return static_cast< storage_type >(BOOST_ATOMIC_INTERLOCKED_EXCHANGE64(&storage, v));
    }

    static BOOST_FORCEINLINE bool compare_exchange_strong(
        storage_type volatile& storage, storage_type& expected, storage_type desired, memory_order, memory_order) BOOST_NOEXCEPT
    {
        storage_type previous = expected;
        storage_type old_val = static_cast< storage_type >(BOOST_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE64(&storage, desired, previous));
        expected = old_val;
        return (previous == old_val);
    }

    static BOOST_FORCEINLINE storage_type fetch_and(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        return static_cast< storage_type >(BOOST_ATOMIC_INTERLOCKED_AND64(&storage, v));
    }

    static BOOST_FORCEINLINE storage_type fetch_or(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        return static_cast< storage_type >(BOOST_ATOMIC_INTERLOCKED_OR64(&storage, v));
    }

    static BOOST_FORCEINLINE storage_type fetch_xor(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        return static_cast< storage_type >(BOOST_ATOMIC_INTERLOCKED_XOR64(&storage, v));
    }
};

#endif

#if defined(BOOST_ATOMIC_DETAIL_X86_HAS_CMPXCHG16B)

template< bool Signed >
struct msvc_dcas_x86_64
{
    typedef typename make_storage_type< 16u, Signed >::type storage_type;
    typedef typename make_storage_type< 16u, Signed >::aligned aligned_storage_type;

    static BOOST_CONSTEXPR_OR_CONST bool is_always_lock_free = true;

    static BOOST_CONSTEXPR_OR_CONST std::size_t storage_size = 16u;
    static BOOST_CONSTEXPR_OR_CONST bool is_signed = Signed;

    static BOOST_FORCEINLINE void store(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        storage_type value = const_cast< storage_type& >(storage);
        while (!BOOST_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE128(&storage, v, &value)) {}
    }

    static BOOST_FORCEINLINE storage_type load(storage_type const volatile& storage, memory_order) BOOST_NOEXCEPT
    {
        storage_type value = storage_type();
        BOOST_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE128(&storage, value, &value);
        return value;
    }

    static BOOST_FORCEINLINE bool compare_exchange_strong(
        storage_type volatile& storage, storage_type& expected, storage_type desired, memory_order, memory_order) BOOST_NOEXCEPT
    {
        return !!BOOST_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE128(&storage, desired, &expected);
    }

    static BOOST_FORCEINLINE bool compare_exchange_weak(
        storage_type volatile& storage, storage_type& expected, storage_type desired, memory_order success_order, memory_order failure_order) BOOST_NOEXCEPT
    {
        return compare_exchange_strong(storage, expected, desired, success_order, failure_order);
    }
};

template< bool Signed >
struct operations< 16u, Signed > :
    public cas_based_operations< cas_based_exchange< msvc_dcas_x86_64< Signed > > >
{
};

#endif // defined(BOOST_ATOMIC_DETAIL_X86_HAS_CMPXCHG16B)

BOOST_FORCEINLINE void thread_fence(memory_order order) BOOST_NOEXCEPT
{
    BOOST_ATOMIC_DETAIL_COMPILER_BARRIER();
    if (order == memory_order_seq_cst)
        msvc_x86_operations_base::hardware_full_fence();
    BOOST_ATOMIC_DETAIL_COMPILER_BARRIER();
}

BOOST_FORCEINLINE void signal_fence(memory_order order) BOOST_NOEXCEPT
{
    if (order != memory_order_relaxed)
        BOOST_ATOMIC_DETAIL_COMPILER_BARRIER();
}

} // namespace detail
} // namespace atomics
} // namespace boost

#if defined(BOOST_MSVC)
#pragma warning(pop)
#endif

#endif // BOOST_ATOMIC_DETAIL_OPS_MSVC_X86_HPP_INCLUDED_
