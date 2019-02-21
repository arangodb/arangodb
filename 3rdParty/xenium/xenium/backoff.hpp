//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef XENIUM_BACKOFF_HPP
#define XENIUM_BACKOFF_HPP

#include <boost/predef.h>
#include <algorithm>

#if BOOST_ARCH_X86
#include <emmintrin.h>
#endif

namespace xenium {
/**
 * @brief Dummy backoff strategy that does nothing.
 */
struct no_backoff
{
  void operator()() {}
};

template <unsigned Max>
struct exponential_backoff {
  static_assert(Max > 0, "Max must be greater than zero. If you don't want to backoff use the `no_backoff` class.");

  void operator()() {
    for (unsigned i = 0; i < count; ++i)
      do_backoff();
    count = std::min(Max, count * 2);
  }

private:
  void do_backoff() {
#if BOOST_ARCH_X86
    _mm_pause();
#else
    #warning "No backoff implementation available."
    std::this_thread::yield;
#endif
  }

  unsigned count = 1;
};

}

#endif
