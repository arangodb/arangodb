//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef XENIUM_ACQUIRE_GUARD_HPP
#define XENIUM_ACQUIRE_GUARD_HPP

#include <atomic>

namespace xenium {

/**
 * @brief Helper function to acquire a `guard_ptr` without having to define the `guard_ptr`
 * instance beforehand.
 * @param p the `concurrent_ptr` from which we want to acquire a safe reference.
 * @param order the `memory_order` that shall be used in the `guard_ptr::acquire` operation.
 * @return a `guard_ptr` to the node that `p` references.
 */
template <typename ConcurrentPtr>
auto acquire_guard(ConcurrentPtr& p, std::memory_order order = std::memory_order_seq_cst)
{
  typename ConcurrentPtr::guard_ptr guard;
  guard.acquire(p, order);
  return guard;
}

}

#endif
