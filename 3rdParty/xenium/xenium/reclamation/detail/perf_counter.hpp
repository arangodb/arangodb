//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef XENIUM_RECLAMATION_DETAIL_PERF_COUNTER_HPP
#define XENIUM_RECLAMATION_DETAIL_PERF_COUNTER_HPP

#include <cstdint>

namespace xenium { namespace reclamation { namespace detail {
#ifdef WITH_PERF_COUNTER
  struct perf_counter
  {
    perf_counter(std::size_t& counter) : counter(counter), cnt() {}
    ~perf_counter() { counter += cnt;  }
    void inc() { ++cnt; }
  private:
    std::size_t& counter;
    std::size_t cnt;
  };

  #define PERF_COUNTER(name, counter) xenium::reclamation::detail::perf_counter name(counter);
  #define INC_PERF_CNT(counter) ++counter;
#else
  struct perf_counter
  {
    perf_counter() {}
    void inc() {}
  };

  #define PERF_COUNTER(name, counter) xenium::reclamation::detail::perf_counter name;
  #define INC_PERF_CNT(counter)
#endif
}}}

#endif
