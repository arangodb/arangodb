//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef XENIUM_ALIGNED_OBJECT_HPP
#define XENIUM_ALIGNED_OBJECT_HPP

#include <boost/align/aligned_alloc.hpp>

namespace xenium {

  /**
   * @brief A small helper class for correctly aligned dynamic allocations of
   * over-aligned types.
   *
   * Dynamic allocation using the standard `operator new` does not consider the
   * alignment of the allocated type. To ensure that dynamic allocations are
   * correctly aligned, classes can inherit from `aligned_object`.
   * If not specified explicitly, `aligned_object` implicitly uses the alignment
   * from `std::alignment_of<Derived>()`.
   * @tparam Derived
   * @tparam Alignment the alignment to use for dynamic allocations.
   * If this value is 0, the alignment is derived from `Derived`. Defaults to 0.
   */
  template <typename Derived, std::size_t Alignment = 0>
  struct aligned_object {
    static void* operator new(size_t sz) {
      constexpr auto alignment = Alignment == 0 ? std::alignment_of<Derived>() : Alignment;
      return boost::alignment::aligned_alloc(alignment, sz);
    }

    static void operator delete(void* p) {
      boost::alignment::aligned_free(p);
    }
  };
}

#endif
