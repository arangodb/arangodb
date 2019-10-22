// Copyright 2018-2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_ALGORITHM_REDUCE_HPP
#define BOOST_HISTOGRAM_ALGORITHM_REDUCE_HPP

#include <boost/assert.hpp>
#include <boost/histogram/axis/traits.hpp>
#include <boost/histogram/detail/axes.hpp>
#include <boost/histogram/detail/cat.hpp>
#include <boost/histogram/detail/make_default.hpp>
#include <boost/histogram/detail/static_if.hpp>
#include <boost/histogram/detail/type_name.hpp>
#include <boost/histogram/fwd.hpp>
#include <boost/histogram/indexed.hpp>
#include <boost/histogram/unsafe_access.hpp>
#include <boost/throw_exception.hpp>
#include <cmath>
#include <initializer_list>
#include <stdexcept>

namespace boost {
namespace histogram {
namespace detail {
struct reduce_option {
  unsigned iaxis = 0;
  bool indices_set = false;
  axis::index_type begin = 0, end = 0;
  bool values_set = false;
  double lower = 0.0, upper = 0.0;
  unsigned merge = 0;
};
} // namespace detail

namespace algorithm {

using reduce_option = detail::reduce_option;

/**
  Shrink and rebin option to be used in reduce().

  To shrink and rebin in one command. Equivalent to passing both the shrink() and the
  rebin() option for the same axis to reduce.

  @param iaxis which axis to operate on.
  @param lower lowest bound that should be kept.
  @param upper highest bound that should be kept. If upper is inside bin interval, the
  whole interval is removed.
  @param merge how many adjacent bins to merge into one.
 */
inline reduce_option shrink_and_rebin(unsigned iaxis, double lower, double upper,
                                      unsigned merge) {
  if (lower == upper)
    BOOST_THROW_EXCEPTION(std::invalid_argument("lower != upper required"));
  if (merge == 0) BOOST_THROW_EXCEPTION(std::invalid_argument("merge > 0 required"));
  return {iaxis, false, 0, 0, true, lower, upper, merge};
}

/**
  Slice and rebin option to be used in reduce().

  To slice and rebin in one command. Equivalent to passing both the slice() and the
  rebin() option for the same axis to reduce.

  @param iaxis which axis to operate on.
  @param begin first index that should be kept.
  @param end one past the last index that should be kept.
  @param merge how many adjacent bins to merge into one.
 */
inline reduce_option slice_and_rebin(unsigned iaxis, axis::index_type begin,
                                     axis::index_type end, unsigned merge) {
  if (!(begin < end))
    BOOST_THROW_EXCEPTION(std::invalid_argument("begin < end required"));
  if (merge == 0) BOOST_THROW_EXCEPTION(std::invalid_argument("merge > 0 required"));
  return {iaxis, true, begin, end, false, 0.0, 0.0, merge};
}

/**
  Shrink option to be used in reduce().

  @param iaxis which axis to operate on.
  @param lower lowest bound that should be kept.
  @param upper highest bound that should be kept. If upper is inside bin interval, the
  whole interval is removed.
 */
inline reduce_option shrink(unsigned iaxis, double lower, double upper) {
  return shrink_and_rebin(iaxis, lower, upper, 1);
}

/**
  Slice option to be used in reduce().

  @param iaxis which axis to operate on.
  @param begin first index that should be kept.
  @param end one past the last index that should be kept.
 */
inline reduce_option slice(unsigned iaxis, axis::index_type begin, axis::index_type end) {
  return slice_and_rebin(iaxis, begin, end, 1);
}

/**
  Rebin option to be used in reduce().

  @param iaxis which axis to operate on.
  @param merge how many adjacent bins to merge into one.
 */
inline reduce_option rebin(unsigned iaxis, unsigned merge) {
  if (merge == 0) BOOST_THROW_EXCEPTION(std::invalid_argument("merge > 0 required"));
  return reduce_option{iaxis, false, 0, 0, false, 0.0, 0.0, merge};
}

/**
  Shrink and rebin option to be used in reduce() (onvenience overload for
  single axis).

  @param lower lowest bound that should be kept.
  @param upper highest bound that should be kept. If upper is inside bin interval, the
  whole interval is removed.
  @param merge how many adjacent bins to merge into one.
*/
inline reduce_option shrink_and_rebin(double lower, double upper, unsigned merge) {
  return shrink_and_rebin(0, lower, upper, merge);
}

/**
  Slice and rebin option to be used in reduce() (convenience for 1D histograms).

  @param begin first index that should be kept.
  @param end one past the last index that should be kept.
  @param merge how many adjacent bins to merge into one.
*/
inline reduce_option slice_and_rebin(axis::index_type begin, axis::index_type end,
                                     unsigned merge) {
  return slice_and_rebin(0, begin, end, merge);
}

/**
  Shrink option to be used in reduce() (convenience for 1D histograms).

  @param lower lowest bound that should be kept.
  @param upper highest bound that should be kept. If upper is inside bin interval, the
  whole interval is removed.
*/
inline reduce_option shrink(double lower, double upper) {
  return shrink(0, lower, upper);
}

/**
  Slice option to be used in reduce() (convenience for 1D histograms).

  @param begin first index that should be kept.
  @param end one past the last index that should be kept.
*/
inline reduce_option slice(axis::index_type begin, axis::index_type end) {
  return slice(0, begin, end);
}

/**
  Rebin option to be used in reduce() (convenience for 1D histograms).

  @param merge how many adjacent bins to merge into one.
*/
inline reduce_option rebin(unsigned merge) { return rebin(0, merge); }

/**
  Shrink, slice, and/or rebin axes of a histogram.

  Returns the reduced copy of the histogram.

  Shrinking only works with axes that accept double values. Some axis types do not support
  the reduce operation, for example, the builtin category axis, which is not ordered.
  Custom axis types must implement a special constructor (see concepts) to be reducible.

  @param hist original histogram.
  @param options iterable sequence of reduce options, generated by shrink_and_rebin(),
  slice_and_rebin(), shrink(), slice(), and rebin().
 */
template <class Histogram, class Iterable, class = detail::requires_iterable<Iterable>>
decltype(auto) reduce(const Histogram& hist, const Iterable& options) {
  const auto& old_axes = unsafe_access::axes(hist);

  auto opts = detail::make_stack_buffer<reduce_option>(old_axes);
  for (const reduce_option& o_in : options) {
    BOOST_ASSERT(o_in.merge > 0);
    if (o_in.iaxis >= hist.rank())
      BOOST_THROW_EXCEPTION(std::invalid_argument("invalid axis index"));
    reduce_option& o_out = opts[o_in.iaxis];
    if (o_out.merge > 0) {
      // some option was already set for this axis, see if we can merge requests
      if (o_in.merge > 1 && o_out.merge > 1)
        BOOST_THROW_EXCEPTION(std::invalid_argument("conflicting merge requests"));
      if ((o_in.indices_set || o_in.values_set) &&
          (o_out.indices_set || o_out.values_set))
        BOOST_THROW_EXCEPTION(
            std::invalid_argument("conflicting slice or shrink requests"));
    }
    if (o_in.values_set) {
      o_out.values_set = true;
      o_out.lower = o_in.lower;
      o_out.upper = o_in.upper;
    } else if (o_in.indices_set) {
      o_out.indices_set = true;
      o_out.begin = o_in.begin;
      o_out.end = o_in.end;
    }
    o_out.merge = std::max(o_in.merge, o_out.merge);
  }

  // make new axes container with default-constructed axis instances
  auto axes = detail::make_default(old_axes);
  detail::static_if<detail::is_tuple<decltype(axes)>>(
      [](auto&, const auto&) {},
      [](auto& axes, const auto& old_axes) {
        axes.reserve(old_axes.size());
        detail::for_each_axis(old_axes, [&axes](const auto& a) {
          axes.emplace_back(detail::make_default(a));
        });
      },
      axes, old_axes);

  // override default-constructed axis instances with modified instances
  unsigned iaxis = 0;
  hist.for_each_axis([&](const auto& a) {
    using A = std::decay_t<decltype(a)>;
    auto& o = opts[iaxis];
    if (o.merge > 0) { // option is set?
      detail::static_if_c<axis::traits::is_reducible<A>::value>(
          [&o](auto&& aout, const auto& ain) {
            using A = std::decay_t<decltype(ain)>;
            if (o.indices_set) {
              o.begin = std::max(0, o.begin);
              o.end = std::min(o.end, ain.size());
            } else {
              o.begin = 0;
              o.end = ain.size();
              if (o.values_set) {
                if (o.lower < o.upper) {
                  while (o.begin != o.end && ain.value(o.begin) < o.lower) ++o.begin;
                  while (o.end != o.begin && ain.value(o.end - 1) >= o.upper) --o.end;
                } else if (o.lower > o.upper) {
                  // for inverted axis::regular
                  while (o.begin != o.end && ain.value(o.begin) > o.lower) ++o.begin;
                  while (o.end != o.begin && ain.value(o.end - 1) <= o.upper) --o.end;
                }
              }
            }
            o.end -= (o.end - o.begin) % o.merge;
            aout = A(ain, o.begin, o.end, o.merge);
          },
          [](auto&&, const auto& ain) {
            using A = std::decay_t<decltype(ain)>;
            BOOST_THROW_EXCEPTION(std::invalid_argument(
                detail::cat(detail::type_name<A>(), " is not reducible")));
          },
          axis::get<A>(detail::axis_get(axes, iaxis)), a);
    } else {
      o.merge = 1;
      o.begin = 0;
      o.end = a.size();
      axis::get<A>(detail::axis_get(axes, iaxis)) = a;
    }
    ++iaxis;
  });

  auto storage = detail::make_default(unsafe_access::storage(hist));
  auto result = Histogram(std::move(axes), std::move(storage));

  auto idx = detail::make_stack_buffer<int>(unsafe_access::axes(result));
  for (auto&& x : indexed(hist, coverage::all)) {
    auto i = idx.begin();
    auto o = opts.begin();
    for (auto j : x.indices()) {
      *i = (j - o->begin);
      if (*i <= -1)
        *i = -1;
      else {
        *i /= o->merge;
        const int end = (o->end - o->begin) / o->merge;
        if (*i > end) *i = end;
      }
      ++i;
      ++o;
    }
    result.at(idx) += *x;
  }

  return result;
}

/**
  Shrink, slice, and/or rebin axes of a histogram.

  Returns the reduced copy of the histogram.

  Shrinking only works with axes that accept double values. Some axis types do not support
  the reduce operation, for example, the builtin category axis, which is not ordered.
  Custom axis types must implement a special constructor (see concepts) to be reducible.

  @param hist original histogram.
  @param opt  reduce option generated by shrink_and_rebin(), shrink(), and rebin().
  @param opts more reduce options.
 */
template <class Histogram, class... Ts>
decltype(auto) reduce(const Histogram& hist, const reduce_option& opt,
                      const Ts&... opts) {
  // this must be in one line, because any of the ts could be a temporary
  return reduce(hist, std::initializer_list<reduce_option>{opt, opts...});
}

} // namespace algorithm
} // namespace histogram
} // namespace boost

#endif
