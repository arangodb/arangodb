/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2020 Andrey Semashev
 */
/*!
 * \file   atomic/detail/wait_operations_fwd.hpp
 *
 * This header contains forward declaration of the \c wait_operations template.
 */

#ifndef BOOST_ATOMIC_DETAIL_WAIT_OPERATIONS_FWD_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_WAIT_OPERATIONS_FWD_HPP_INCLUDED_

#include <cstddef>
#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/header.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {
namespace atomics {
namespace detail {

template<
    typename Base,
    std::size_t Size = sizeof(typename Base::storage_type),
    bool AlwaysLockFree = Base::is_always_lock_free,
    bool Interprocess = Base::is_interprocess
>
struct wait_operations;

} // namespace detail
} // namespace atomics
} // namespace boost

#include <boost/atomic/detail/footer.hpp>

#endif // BOOST_ATOMIC_DETAIL_WAIT_OPERATIONS_FWD_HPP_INCLUDED_
