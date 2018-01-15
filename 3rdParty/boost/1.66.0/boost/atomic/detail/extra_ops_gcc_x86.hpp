/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2015 Andrey Semashev
 */
/*!
 * \file   atomic/detail/extra_ops_gcc_x86.hpp
 *
 * This header contains implementation of the extra atomic operations for x86.
 */

#ifndef BOOST_ATOMIC_DETAIL_EXTRA_OPS_GCC_X86_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_EXTRA_OPS_GCC_X86_HPP_INCLUDED_

#include <cstddef>
#include <boost/memory_order.hpp>
#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/storage_type.hpp>
#include <boost/atomic/detail/extra_operations_fwd.hpp>
#include <boost/atomic/capabilities.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {
namespace atomics {
namespace detail {

template< typename Base >
struct gcc_x86_extra_operations_common :
    public Base
{
    typedef Base base_type;
    typedef typename base_type::storage_type storage_type;

    static BOOST_FORCEINLINE bool bit_test_and_set(storage_type volatile& storage, unsigned int bit_number, memory_order) BOOST_NOEXCEPT
    {
        bool res;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; bts %[bit_number], %[storage]\n\t"
            : [storage] "+m" (storage), [result] "=@ccc" (res)
            : [bit_number] "Kq" (bit_number)
            : "memory"
        );
#else
        __asm__ __volatile__
        (
            "lock; bts %[bit_number], %[storage]\n\t"
            "setc %[result]\n\t"
            : [storage] "+m" (storage), [result] "=q" (res)
            : [bit_number] "Kq" (bit_number)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#endif
        return res;
    }

    static BOOST_FORCEINLINE bool bit_test_and_reset(storage_type volatile& storage, unsigned int bit_number, memory_order) BOOST_NOEXCEPT
    {
        bool res;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; btr %[bit_number], %[storage]\n\t"
            : [storage] "+m" (storage), [result] "=@ccc" (res)
            : [bit_number] "Kq" (bit_number)
            : "memory"
        );
#else
        __asm__ __volatile__
        (
            "lock; btr %[bit_number], %[storage]\n\t"
            "setc %[result]\n\t"
            : [storage] "+m" (storage), [result] "=q" (res)
            : [bit_number] "Kq" (bit_number)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#endif
        return res;
    }

    static BOOST_FORCEINLINE bool bit_test_and_complement(storage_type volatile& storage, unsigned int bit_number, memory_order) BOOST_NOEXCEPT
    {
        bool res;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; btc %[bit_number], %[storage]\n\t"
            : [storage] "+m" (storage), [result] "=@ccc" (res)
            : [bit_number] "Kq" (bit_number)
            : "memory"
        );
#else
        __asm__ __volatile__
        (
            "lock; btc %[bit_number], %[storage]\n\t"
            "setc %[result]\n\t"
            : [storage] "+m" (storage), [result] "=q" (res)
            : [bit_number] "Kq" (bit_number)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#endif
        return res;
    }
};

template< typename Base, bool Signed >
struct extra_operations< Base, 1u, Signed > :
    public gcc_x86_extra_operations_common< Base >
{
    typedef gcc_x86_extra_operations_common< Base > base_type;
    typedef typename base_type::storage_type storage_type;
    typedef typename make_storage_type< 4u, Signed >::type temp_storage_type;

#define BOOST_ATOMIC_DETAIL_CAS_LOOP(op, result)\
    temp_storage_type new_val;\
    __asm__ __volatile__\
    (\
        ".align 16\n\t"\
        "1: movzbl %[res], %2\n\t"\
        op " %b2\n\t"\
        "lock; cmpxchgb %b2, %[storage]\n\t"\
        "jne 1b"\
        : [res] "+a" (result), [storage] "+m" (storage), "=&q" (new_val)\
        : \
        : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"\
    )

    static BOOST_FORCEINLINE storage_type fetch_negate(storage_type volatile& storage, memory_order) BOOST_NOEXCEPT
    {
        storage_type res = storage;
        BOOST_ATOMIC_DETAIL_CAS_LOOP("negb", res);
        return res;
    }

    static BOOST_FORCEINLINE storage_type fetch_complement(storage_type volatile& storage, memory_order) BOOST_NOEXCEPT
    {
        storage_type res = storage;
        BOOST_ATOMIC_DETAIL_CAS_LOOP("notb", res);
        return res;
    }

#undef BOOST_ATOMIC_DETAIL_CAS_LOOP

    static BOOST_FORCEINLINE void opaque_add(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        if (BOOST_ATOMIC_DETAIL_IS_CONSTANT(v) && v == 1)
        {
            __asm__ __volatile__
            (
                "lock; incb %[storage]\n\t"
                : [storage] "+m" (storage)
                :
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
        else
        {
            __asm__ __volatile__
            (
                "lock; addb %[argument], %[storage]\n\t"
                : [storage] "+m" (storage)
                : [argument] "iq" (v)
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
    }

    static BOOST_FORCEINLINE void opaque_sub(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        if (BOOST_ATOMIC_DETAIL_IS_CONSTANT(v) && v == 1)
        {
            __asm__ __volatile__
            (
                "lock; decb %[storage]\n\t"
                : [storage] "+m" (storage)
                :
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
        else
        {
            __asm__ __volatile__
            (
                "lock; subb %[argument], %[storage]\n\t"
                : [storage] "+m" (storage)
                : [argument] "iq" (v)
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
    }

    static BOOST_FORCEINLINE void opaque_negate(storage_type volatile& storage, memory_order) BOOST_NOEXCEPT
    {
        __asm__ __volatile__
        (
            "lock; negb %[storage]\n\t"
            : [storage] "+m" (storage)
            :
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
    }

    static BOOST_FORCEINLINE void opaque_and(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        __asm__ __volatile__
        (
            "lock; andb %[argument], %[storage]\n\t"
            : [storage] "+m" (storage)
            : [argument] "iq" (v)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
    }

    static BOOST_FORCEINLINE void opaque_or(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        __asm__ __volatile__
        (
            "lock; orb %[argument], %[storage]\n\t"
            : [storage] "+m" (storage)
            : [argument] "iq" (v)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
    }

    static BOOST_FORCEINLINE void opaque_xor(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        __asm__ __volatile__
        (
            "lock; xorb %[argument], %[storage]\n\t"
            : [storage] "+m" (storage)
            : [argument] "iq" (v)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
    }

    static BOOST_FORCEINLINE void opaque_complement(storage_type volatile& storage, memory_order) BOOST_NOEXCEPT
    {
        __asm__ __volatile__
        (
            "lock; notb %[storage]\n\t"
            : [storage] "+m" (storage)
            :
            : "memory"
        );
    }

    static BOOST_FORCEINLINE bool add_and_test(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        bool res;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        if (BOOST_ATOMIC_DETAIL_IS_CONSTANT(v) && v == 1)
        {
            __asm__ __volatile__
            (
                "lock; incb %[storage]\n\t"
                : [storage] "+m" (storage), [result] "=@ccz" (res)
                :
                : "memory"
            );
        }
        else
        {
            __asm__ __volatile__
            (
                "lock; addb %[argument], %[storage]\n\t"
                : [storage] "+m" (storage), [result] "=@ccz" (res)
                : [argument] "iq" (v)
                : "memory"
            );
        }
#else
        if (BOOST_ATOMIC_DETAIL_IS_CONSTANT(v) && v == 1)
        {
            __asm__ __volatile__
            (
                "lock; incb %[storage]\n\t"
                "setz %[result]\n\t"
                : [storage] "+m" (storage), [result] "=q" (res)
                :
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
        else
        {
            __asm__ __volatile__
            (
                "lock; addb %[argument], %[storage]\n\t"
                "setz %[result]\n\t"
                : [storage] "+m" (storage), [result] "=q" (res)
                : [argument] "iq" (v)
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
#endif
        return res;
    }

    static BOOST_FORCEINLINE bool sub_and_test(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        bool res;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        if (BOOST_ATOMIC_DETAIL_IS_CONSTANT(v) && v == 1)
        {
            __asm__ __volatile__
            (
                "lock; decb %[storage]\n\t"
                : [storage] "+m" (storage), [result] "=@ccz" (res)
                :
                : "memory"
            );
        }
        else
        {
            __asm__ __volatile__
            (
                "lock; subb %[argument], %[storage]\n\t"
                : [storage] "+m" (storage), [result] "=@ccz" (res)
                : [argument] "iq" (v)
                : "memory"
            );
        }
#else
        if (BOOST_ATOMIC_DETAIL_IS_CONSTANT(v) && v == 1)
        {
            __asm__ __volatile__
            (
                "lock; decb %[storage]\n\t"
                "setz %[result]\n\t"
                : [storage] "+m" (storage), [result] "=q" (res)
                :
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
        else
        {
            __asm__ __volatile__
            (
                "lock; subb %[argument], %[storage]\n\t"
                "setz %[result]\n\t"
                : [storage] "+m" (storage), [result] "=q" (res)
                : [argument] "iq" (v)
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
#endif
        return res;
    }

    static BOOST_FORCEINLINE bool and_and_test(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        bool res;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; andb %[argument], %[storage]\n\t"
            : [storage] "+m" (storage), [result] "=@ccz" (res)
            : [argument] "iq" (v)
            : "memory"
        );
#else
        __asm__ __volatile__
        (
            "lock; andb %[argument], %[storage]\n\t"
            "setz %[result]\n\t"
            : [storage] "+m" (storage), [result] "=q" (res)
            : [argument] "iq" (v)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#endif
        return res;
    }

    static BOOST_FORCEINLINE bool or_and_test(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        bool res;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; orb %[argument], %[storage]\n\t"
            : [storage] "+m" (storage), [result] "=@ccz" (res)
            : [argument] "iq" (v)
            : "memory"
        );
#else
        __asm__ __volatile__
        (
            "lock; orb %[argument], %[storage]\n\t"
            "setz %[result]\n\t"
            : [storage] "+m" (storage), [result] "=q" (res)
            : [argument] "iq" (v)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#endif
        return res;
    }

    static BOOST_FORCEINLINE bool xor_and_test(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        bool res;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; xorb %[argument], %[storage]\n\t"
            : [storage] "+m" (storage), [result] "=@ccz" (res)
            : [argument] "iq" (v)
            : "memory"
        );
#else
        __asm__ __volatile__
        (
            "lock; xorb %[argument], %[storage]\n\t"
            "setz %[result]\n\t"
            : [storage] "+m" (storage), [result] "=q" (res)
            : [argument] "iq" (v)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#endif
        return res;
    }
};

template< typename Base, bool Signed >
struct extra_operations< Base, 2u, Signed > :
    public gcc_x86_extra_operations_common< Base >
{
    typedef gcc_x86_extra_operations_common< Base > base_type;
    typedef typename base_type::storage_type storage_type;
    typedef typename make_storage_type< 4u, Signed >::type temp_storage_type;

#define BOOST_ATOMIC_DETAIL_CAS_LOOP(op, result)\
    temp_storage_type new_val;\
    __asm__ __volatile__\
    (\
        ".align 16\n\t"\
        "1: movzwl %[res], %2\n\t"\
        op " %w2\n\t"\
        "lock; cmpxchgw %w2, %[storage]\n\t"\
        "jne 1b"\
        : [res] "+a" (result), [storage] "+m" (storage), "=&q" (new_val)\
        : \
        : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"\
    )

    static BOOST_FORCEINLINE storage_type fetch_negate(storage_type volatile& storage, memory_order) BOOST_NOEXCEPT
    {
        storage_type res = storage;
        BOOST_ATOMIC_DETAIL_CAS_LOOP("negw", res);
        return res;
    }

    static BOOST_FORCEINLINE storage_type fetch_complement(storage_type volatile& storage, memory_order) BOOST_NOEXCEPT
    {
        storage_type res = storage;
        BOOST_ATOMIC_DETAIL_CAS_LOOP("notw", res);
        return res;
    }

#undef BOOST_ATOMIC_DETAIL_CAS_LOOP

    static BOOST_FORCEINLINE void opaque_add(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        if (BOOST_ATOMIC_DETAIL_IS_CONSTANT(v) && v == 1)
        {
            __asm__ __volatile__
            (
                "lock; incw %[storage]\n\t"
                : [storage] "+m" (storage)
                :
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
        else
        {
            __asm__ __volatile__
            (
                "lock; addw %[argument], %[storage]\n\t"
                : [storage] "+m" (storage)
                : [argument] "iq" (v)
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
    }

    static BOOST_FORCEINLINE void opaque_sub(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        if (BOOST_ATOMIC_DETAIL_IS_CONSTANT(v) && v == 1)
        {
            __asm__ __volatile__
            (
                "lock; decw %[storage]\n\t"
                : [storage] "+m" (storage)
                :
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
        else
        {
            __asm__ __volatile__
            (
                "lock; subw %[argument], %[storage]\n\t"
                : [storage] "+m" (storage)
                : [argument] "iq" (v)
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
    }

    static BOOST_FORCEINLINE void opaque_negate(storage_type volatile& storage, memory_order) BOOST_NOEXCEPT
    {
        __asm__ __volatile__
        (
            "lock; negw %[storage]\n\t"
            : [storage] "+m" (storage)
            :
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
    }

    static BOOST_FORCEINLINE void opaque_and(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        __asm__ __volatile__
        (
            "lock; andw %[argument], %[storage]\n\t"
            : [storage] "+m" (storage)
            : [argument] "iq" (v)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
    }

    static BOOST_FORCEINLINE void opaque_or(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        __asm__ __volatile__
        (
            "lock; orw %[argument], %[storage]\n\t"
            : [storage] "+m" (storage)
            : [argument] "iq" (v)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
    }

    static BOOST_FORCEINLINE void opaque_xor(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        __asm__ __volatile__
        (
            "lock; xorw %[argument], %[storage]\n\t"
            : [storage] "+m" (storage)
            : [argument] "iq" (v)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
    }

    static BOOST_FORCEINLINE void opaque_complement(storage_type volatile& storage, memory_order) BOOST_NOEXCEPT
    {
        __asm__ __volatile__
        (
            "lock; notw %[storage]\n\t"
            : [storage] "+m" (storage)
            :
            : "memory"
        );
    }

    static BOOST_FORCEINLINE bool add_and_test(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        bool res;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        if (BOOST_ATOMIC_DETAIL_IS_CONSTANT(v) && v == 1)
        {
            __asm__ __volatile__
            (
                "lock; incw %[storage]\n\t"
                : [storage] "+m" (storage), [result] "=@ccz" (res)
                :
                : "memory"
            );
        }
        else
        {
            __asm__ __volatile__
            (
                "lock; addw %[argument], %[storage]\n\t"
                : [storage] "+m" (storage), [result] "=@ccz" (res)
                : [argument] "iq" (v)
                : "memory"
            );
        }
#else
        if (BOOST_ATOMIC_DETAIL_IS_CONSTANT(v) && v == 1)
        {
            __asm__ __volatile__
            (
                "lock; incw %[storage]\n\t"
                "setz %[result]\n\t"
                : [storage] "+m" (storage), [result] "=q" (res)
                :
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
        else
        {
            __asm__ __volatile__
            (
                "lock; addw %[argument], %[storage]\n\t"
                "setz %[result]\n\t"
                : [storage] "+m" (storage), [result] "=q" (res)
                : [argument] "iq" (v)
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
#endif
        return res;
    }

    static BOOST_FORCEINLINE bool sub_and_test(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        bool res;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        if (BOOST_ATOMIC_DETAIL_IS_CONSTANT(v) && v == 1)
        {
            __asm__ __volatile__
            (
                "lock; decw %[storage]\n\t"
                : [storage] "+m" (storage), [result] "=@ccz" (res)
                :
                : "memory"
            );
        }
        else
        {
            __asm__ __volatile__
            (
                "lock; subw %[argument], %[storage]\n\t"
                : [storage] "+m" (storage), [result] "=@ccz" (res)
                : [argument] "iq" (v)
                : "memory"
            );
        }
#else
        if (BOOST_ATOMIC_DETAIL_IS_CONSTANT(v) && v == 1)
        {
            __asm__ __volatile__
            (
                "lock; decw %[storage]\n\t"
                "setz %[result]\n\t"
                : [storage] "+m" (storage), [result] "=q" (res)
                :
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
        else
        {
            __asm__ __volatile__
            (
                "lock; subw %[argument], %[storage]\n\t"
                "setz %[result]\n\t"
                : [storage] "+m" (storage), [result] "=q" (res)
                : [argument] "iq" (v)
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
#endif
        return res;
    }

    static BOOST_FORCEINLINE bool and_and_test(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        bool res;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; andw %[argument], %[storage]\n\t"
            : [storage] "+m" (storage), [result] "=@ccz" (res)
            : [argument] "iq" (v)
            : "memory"
        );
#else
        __asm__ __volatile__
        (
            "lock; andw %[argument], %[storage]\n\t"
            "setz %[result]\n\t"
            : [storage] "+m" (storage), [result] "=q" (res)
            : [argument] "iq" (v)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#endif
        return res;
    }

    static BOOST_FORCEINLINE bool or_and_test(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        bool res;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; orw %[argument], %[storage]\n\t"
            : [storage] "+m" (storage), [result] "=@ccz" (res)
            : [argument] "iq" (v)
            : "memory"
        );
#else
        __asm__ __volatile__
        (
            "lock; orw %[argument], %[storage]\n\t"
            "setz %[result]\n\t"
            : [storage] "+m" (storage), [result] "=q" (res)
            : [argument] "iq" (v)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#endif
        return res;
    }

    static BOOST_FORCEINLINE bool xor_and_test(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        bool res;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; xorw %[argument], %[storage]\n\t"
            : [storage] "+m" (storage), [result] "=@ccz" (res)
            : [argument] "iq" (v)
            : "memory"
        );
#else
        __asm__ __volatile__
        (
            "lock; xorw %[argument], %[storage]\n\t"
            "setz %[result]\n\t"
            : [storage] "+m" (storage), [result] "=q" (res)
            : [argument] "iq" (v)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#endif
        return res;
    }
};

template< typename Base, bool Signed >
struct extra_operations< Base, 4u, Signed > :
    public gcc_x86_extra_operations_common< Base >
{
    typedef gcc_x86_extra_operations_common< Base > base_type;
    typedef typename base_type::storage_type storage_type;

#define BOOST_ATOMIC_DETAIL_CAS_LOOP(op, result)\
    storage_type new_val;\
    __asm__ __volatile__\
    (\
        ".align 16\n\t"\
        "1: mov %[res], %[new_val]\n\t"\
        op " %[new_val]\n\t"\
        "lock; cmpxchgl %[new_val], %[storage]\n\t"\
        "jne 1b"\
        : [res] "+a" (result), [storage] "+m" (storage), [new_val] "=&r" (new_val)\
        : \
        : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"\
    )

    static BOOST_FORCEINLINE storage_type fetch_negate(storage_type volatile& storage, memory_order) BOOST_NOEXCEPT
    {
        storage_type res = storage;
        BOOST_ATOMIC_DETAIL_CAS_LOOP("negl", res);
        return res;
    }

    static BOOST_FORCEINLINE storage_type fetch_complement(storage_type volatile& storage, memory_order) BOOST_NOEXCEPT
    {
        storage_type res = storage;
        BOOST_ATOMIC_DETAIL_CAS_LOOP("notl", res);
        return res;
    }

#undef BOOST_ATOMIC_DETAIL_CAS_LOOP

    static BOOST_FORCEINLINE void opaque_add(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        if (BOOST_ATOMIC_DETAIL_IS_CONSTANT(v) && v == 1)
        {
            __asm__ __volatile__
            (
                "lock; incl %[storage]\n\t"
                : [storage] "+m" (storage)
                :
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
        else
        {
            __asm__ __volatile__
            (
                "lock; addl %[argument], %[storage]\n\t"
                : [storage] "+m" (storage)
                : [argument] "ir" (v)
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
    }

    static BOOST_FORCEINLINE void opaque_sub(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        if (BOOST_ATOMIC_DETAIL_IS_CONSTANT(v) && v == 1)
        {
            __asm__ __volatile__
            (
                "lock; decl %[storage]\n\t"
                : [storage] "+m" (storage)
                :
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
        else
        {
            __asm__ __volatile__
            (
                "lock; subl %[argument], %[storage]\n\t"
                : [storage] "+m" (storage)
                : [argument] "ir" (v)
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
    }

    static BOOST_FORCEINLINE void opaque_negate(storage_type volatile& storage, memory_order) BOOST_NOEXCEPT
    {
        __asm__ __volatile__
        (
            "lock; negl %[storage]\n\t"
            : [storage] "+m" (storage)
            :
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
    }

    static BOOST_FORCEINLINE void opaque_and(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        __asm__ __volatile__
        (
            "lock; andl %[argument], %[storage]\n\t"
            : [storage] "+m" (storage)
            : [argument] "ir" (v)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
    }

    static BOOST_FORCEINLINE void opaque_or(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        __asm__ __volatile__
        (
            "lock; orl %[argument], %[storage]\n\t"
            : [storage] "+m" (storage)
            : [argument] "ir" (v)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
    }

    static BOOST_FORCEINLINE void opaque_xor(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        __asm__ __volatile__
        (
            "lock; xorl %[argument], %[storage]\n\t"
            : [storage] "+m" (storage)
            : [argument] "ir" (v)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
    }

    static BOOST_FORCEINLINE void opaque_complement(storage_type volatile& storage, memory_order) BOOST_NOEXCEPT
    {
        __asm__ __volatile__
        (
            "lock; notl %[storage]\n\t"
            : [storage] "+m" (storage)
            :
            : "memory"
        );
    }

    static BOOST_FORCEINLINE bool add_and_test(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        bool res;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        if (BOOST_ATOMIC_DETAIL_IS_CONSTANT(v) && v == 1)
        {
            __asm__ __volatile__
            (
                "lock; incl %[storage]\n\t"
                : [storage] "+m" (storage), [result] "=@ccz" (res)
                :
                : "memory"
            );
        }
        else
        {
            __asm__ __volatile__
            (
                "lock; addl %[argument], %[storage]\n\t"
                : [storage] "+m" (storage), [result] "=@ccz" (res)
                : [argument] "ir" (v)
                : "memory"
            );
        }
#else
        if (BOOST_ATOMIC_DETAIL_IS_CONSTANT(v) && v == 1)
        {
            __asm__ __volatile__
            (
                "lock; incl %[storage]\n\t"
                "setz %[result]\n\t"
                : [storage] "+m" (storage), [result] "=q" (res)
                :
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
        else
        {
            __asm__ __volatile__
            (
                "lock; addl %[argument], %[storage]\n\t"
                "setz %[result]\n\t"
                : [storage] "+m" (storage), [result] "=q" (res)
                : [argument] "ir" (v)
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
#endif
        return res;
    }

    static BOOST_FORCEINLINE bool sub_and_test(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        bool res;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        if (BOOST_ATOMIC_DETAIL_IS_CONSTANT(v) && v == 1)
        {
            __asm__ __volatile__
            (
                "lock; decl %[storage]\n\t"
                : [storage] "+m" (storage), [result] "=@ccz" (res)
                :
                : "memory"
            );
        }
        else
        {
            __asm__ __volatile__
            (
                "lock; subl %[argument], %[storage]\n\t"
                : [storage] "+m" (storage), [result] "=@ccz" (res)
                : [argument] "ir" (v)
                : "memory"
            );
        }
#else
        if (BOOST_ATOMIC_DETAIL_IS_CONSTANT(v) && v == 1)
        {
            __asm__ __volatile__
            (
                "lock; decl %[storage]\n\t"
                "setz %[result]\n\t"
                : [storage] "+m" (storage), [result] "=q" (res)
                :
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
        else
        {
            __asm__ __volatile__
            (
                "lock; subl %[argument], %[storage]\n\t"
                "setz %[result]\n\t"
                : [storage] "+m" (storage), [result] "=q" (res)
                : [argument] "ir" (v)
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
#endif
        return res;
    }

    static BOOST_FORCEINLINE bool and_and_test(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        bool res;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; andl %[argument], %[storage]\n\t"
            : [storage] "+m" (storage), [result] "=@ccz" (res)
            : [argument] "ir" (v)
            : "memory"
        );
#else
        __asm__ __volatile__
        (
            "lock; andl %[argument], %[storage]\n\t"
            "setz %[result]\n\t"
            : [storage] "+m" (storage), [result] "=q" (res)
            : [argument] "ir" (v)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#endif
        return res;
    }

    static BOOST_FORCEINLINE bool or_and_test(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        bool res;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; orl %[argument], %[storage]\n\t"
            : [storage] "+m" (storage), [result] "=@ccz" (res)
            : [argument] "ir" (v)
            : "memory"
        );
#else
        __asm__ __volatile__
        (
            "lock; orl %[argument], %[storage]\n\t"
            "setz %[result]\n\t"
            : [storage] "+m" (storage), [result] "=q" (res)
            : [argument] "ir" (v)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#endif
        return res;
    }

    static BOOST_FORCEINLINE bool xor_and_test(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        bool res;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; xorl %[argument], %[storage]\n\t"
            : [storage] "+m" (storage), [result] "=@ccz" (res)
            : [argument] "ir" (v)
            : "memory"
        );
#else
        __asm__ __volatile__
        (
            "lock; xorl %[argument], %[storage]\n\t"
            "setz %[result]\n\t"
            : [storage] "+m" (storage), [result] "=q" (res)
            : [argument] "ir" (v)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#endif
        return res;
    }
};

#if defined(__x86_64__)

template< typename Base, bool Signed >
struct extra_operations< Base, 8u, Signed > :
    public gcc_x86_extra_operations_common< Base >
{
    typedef gcc_x86_extra_operations_common< Base > base_type;
    typedef typename base_type::storage_type storage_type;

#define BOOST_ATOMIC_DETAIL_CAS_LOOP(op, result)\
    storage_type new_val;\
    __asm__ __volatile__\
    (\
        ".align 16\n\t"\
        "1: mov %[res], %[new_val]\n\t"\
        op " %[new_val]\n\t"\
        "lock; cmpxchgq %[new_val], %[storage]\n\t"\
        "jne 1b"\
        : [res] "+a" (result), [storage] "+m" (storage), [new_val] "=&r" (new_val)\
        : \
        : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"\
    )

    static BOOST_FORCEINLINE storage_type fetch_negate(storage_type volatile& storage, memory_order) BOOST_NOEXCEPT
    {
        storage_type res = storage;
        BOOST_ATOMIC_DETAIL_CAS_LOOP("negq", res);
        return res;
    }

    static BOOST_FORCEINLINE storage_type fetch_complement(storage_type volatile& storage, memory_order) BOOST_NOEXCEPT
    {
        storage_type res = storage;
        BOOST_ATOMIC_DETAIL_CAS_LOOP("notq", res);
        return res;
    }

#undef BOOST_ATOMIC_DETAIL_CAS_LOOP

    static BOOST_FORCEINLINE void opaque_add(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        if (BOOST_ATOMIC_DETAIL_IS_CONSTANT(v) && v == 1)
        {
            __asm__ __volatile__
            (
                "lock; incq %[storage]\n\t"
                : [storage] "+m" (storage)
                :
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
        else
        {
            __asm__ __volatile__
            (
                "lock; addq %[argument], %[storage]\n\t"
                : [storage] "+m" (storage)
                : [argument] "er" (v)
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
    }

    static BOOST_FORCEINLINE void opaque_sub(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        if (BOOST_ATOMIC_DETAIL_IS_CONSTANT(v) && v == 1)
        {
            __asm__ __volatile__
            (
                "lock; decq %[storage]\n\t"
                : [storage] "+m" (storage)
                :
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
        else
        {
            __asm__ __volatile__
            (
                "lock; subq %[argument], %[storage]\n\t"
                : [storage] "+m" (storage)
                : [argument] "er" (v)
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
    }

    static BOOST_FORCEINLINE void opaque_negate(storage_type volatile& storage, memory_order) BOOST_NOEXCEPT
    {
        __asm__ __volatile__
        (
            "lock; negq %[storage]\n\t"
            : [storage] "+m" (storage)
            :
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
    }

    static BOOST_FORCEINLINE void opaque_and(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        __asm__ __volatile__
        (
            "lock; andq %[argument], %[storage]\n\t"
            : [storage] "+m" (storage)
            : [argument] "er" (v)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
    }

    static BOOST_FORCEINLINE void opaque_or(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        __asm__ __volatile__
        (
            "lock; orq %[argument], %[storage]\n\t"
            : [storage] "+m" (storage)
            : [argument] "er" (v)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
    }

    static BOOST_FORCEINLINE void opaque_xor(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        __asm__ __volatile__
        (
            "lock; xorq %[argument], %[storage]\n\t"
            : [storage] "+m" (storage)
            : [argument] "er" (v)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
    }

    static BOOST_FORCEINLINE void opaque_complement(storage_type volatile& storage, memory_order) BOOST_NOEXCEPT
    {
        __asm__ __volatile__
        (
            "lock; notq %[storage]\n\t"
            : [storage] "+m" (storage)
            :
            : "memory"
        );
    }

    static BOOST_FORCEINLINE bool add_and_test(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        bool res;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        if (BOOST_ATOMIC_DETAIL_IS_CONSTANT(v) && v == 1)
        {
            __asm__ __volatile__
            (
                "lock; incq %[storage]\n\t"
                : [storage] "+m" (storage), [result] "=@ccz" (res)
                :
                : "memory"
            );
        }
        else
        {
            __asm__ __volatile__
            (
                "lock; addq %[argument], %[storage]\n\t"
                : [storage] "+m" (storage), [result] "=@ccz" (res)
                : [argument] "er" (v)
                : "memory"
            );
        }
#else
        if (BOOST_ATOMIC_DETAIL_IS_CONSTANT(v) && v == 1)
        {
            __asm__ __volatile__
            (
                "lock; incq %[storage]\n\t"
                "setz %[result]\n\t"
                : [storage] "+m" (storage), [result] "=q" (res)
                :
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
        else
        {
            __asm__ __volatile__
            (
                "lock; addq %[argument], %[storage]\n\t"
                "setz %[result]\n\t"
                : [storage] "+m" (storage), [result] "=q" (res)
                : [argument] "er" (v)
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
#endif
        return res;
    }

    static BOOST_FORCEINLINE bool sub_and_test(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        bool res;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        if (BOOST_ATOMIC_DETAIL_IS_CONSTANT(v) && v == 1)
        {
            __asm__ __volatile__
            (
                "lock; decq %[storage]\n\t"
                : [storage] "+m" (storage), [result] "=@ccz" (res)
                :
                : "memory"
            );
        }
        else
        {
            __asm__ __volatile__
            (
                "lock; subq %[argument], %[storage]\n\t"
                : [storage] "+m" (storage), [result] "=@ccz" (res)
                : [argument] "er" (v)
                : "memory"
            );
        }
#else
        if (BOOST_ATOMIC_DETAIL_IS_CONSTANT(v) && v == 1)
        {
            __asm__ __volatile__
            (
                "lock; decq %[storage]\n\t"
                "setz %[result]\n\t"
                : [storage] "+m" (storage), [result] "=q" (res)
                :
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
        else
        {
            __asm__ __volatile__
            (
                "lock; subq %[argument], %[storage]\n\t"
                "setz %[result]\n\t"
                : [storage] "+m" (storage), [result] "=q" (res)
                : [argument] "er" (v)
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
        }
#endif
        return res;
    }

    static BOOST_FORCEINLINE bool and_and_test(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        bool res;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; andq %[argument], %[storage]\n\t"
            : [storage] "+m" (storage), [result] "=@ccz" (res)
            : [argument] "er" (v)
            : "memory"
        );
#else
        __asm__ __volatile__
        (
            "lock; andq %[argument], %[storage]\n\t"
            "setz %[result]\n\t"
            : [storage] "+m" (storage), [result] "=q" (res)
            : [argument] "er" (v)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#endif
        return res;
    }

    static BOOST_FORCEINLINE bool or_and_test(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        bool res;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; orq %[argument], %[storage]\n\t"
            : [storage] "+m" (storage), [result] "=@ccz" (res)
            : [argument] "er" (v)
            : "memory"
        );
#else
        __asm__ __volatile__
        (
            "lock; orq %[argument], %[storage]\n\t"
            "setz %[result]\n\t"
            : [storage] "+m" (storage), [result] "=q" (res)
            : [argument] "er" (v)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#endif
        return res;
    }

    static BOOST_FORCEINLINE bool xor_and_test(storage_type volatile& storage, storage_type v, memory_order) BOOST_NOEXCEPT
    {
        bool res;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; xorq %[argument], %[storage]\n\t"
            : [storage] "+m" (storage), [result] "=@ccz" (res)
            : [argument] "er" (v)
            : "memory"
        );
#else
        __asm__ __volatile__
        (
            "lock; xorq %[argument], %[storage]\n\t"
            "setz %[result]\n\t"
            : [storage] "+m" (storage), [result] "=q" (res)
            : [argument] "er" (v)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#endif
        return res;
    }
};

#endif // defined(__x86_64__)

} // namespace detail
} // namespace atomics
} // namespace boost

#endif // BOOST_ATOMIC_DETAIL_EXTRA_OPS_GCC_X86_HPP_INCLUDED_
