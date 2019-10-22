// Copyright 2015-2016 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_INDEXED_HPP
#define BOOST_HISTOGRAM_INDEXED_HPP

#include <array>
#include <boost/histogram/axis/traits.hpp>
#include <boost/histogram/detail/attribute.hpp>
#include <boost/histogram/detail/axes.hpp>
#include <boost/histogram/detail/iterator_adaptor.hpp>
#include <boost/histogram/detail/operators.hpp>
#include <boost/histogram/fwd.hpp>
#include <type_traits>
#include <utility>

namespace boost {
namespace histogram {

/** Coverage mode of the indexed range generator.

  Defines options for the iteration strategy.
*/
enum class coverage {
  inner, /*!< iterate over inner bins, exclude underflow and overflow */
  all,   /*!< iterate over all bins, including underflow and overflow */
};

/** Input iterator range over histogram bins with multi-dimensional index.

  The iterator returned by begin() can only be incremented. begin() may only be called
  once, calling it a second time returns the end() iterator. If several copies of the
  input iterators exist, the other copies become invalid if one of them is incremented.
*/
template <class Histogram>
class BOOST_HISTOGRAM_NODISCARD indexed_range {
private:
  using histogram_type = Histogram;
  static constexpr std::size_t buffer_size =
      detail::buffer_size<typename std::remove_const_t<histogram_type>::axes_type>::value;

public:
  using value_iterator = decltype(std::declval<histogram_type>().begin());
  using value_reference = typename value_iterator::reference;
  using value_type = typename value_iterator::value_type;

  class iterator;
  using range_iterator = iterator; ///< deprecated

  /** Pointer-like class to access value and index of current cell.

    Its methods provide access to the current indices and bins and it acts like a pointer
    to the cell value. To interoperate with the algorithms of the standard library, the
    accessor is implicitly convertible to a cell value. Assignments and comparisons
    are passed through to the cell. The accessor is coupled to its parent
    iterator. Moving the parent iterator forward also updates the linked
    accessor. Accessors are not copyable. They cannot be stored in containers, but
    range_iterators can be stored.
  */
  class accessor : detail::mirrored<accessor, void> {
  public:
    /// Array-like view into the current multi-dimensional index.
    class index_view {
      using index_pointer = const typename iterator::index_data*;

    public:
      using const_reference = const axis::index_type&;
      using reference = const_reference; ///< deprecated

      /// implementation detail
      class const_iterator
          : public detail::iterator_adaptor<const_iterator, index_pointer,
                                            const_reference> {
      public:
        const_reference operator*() const noexcept { return const_iterator::base()->idx; }

      private:
        explicit const_iterator(index_pointer i) noexcept
            : const_iterator::iterator_adaptor_(i) {}

        friend class index_view;
      };

      const_iterator begin() const noexcept { return const_iterator{begin_}; }
      const_iterator end() const noexcept { return const_iterator{end_}; }
      std::size_t size() const noexcept {
        return static_cast<std::size_t>(end_ - begin_);
      }
      const_reference operator[](unsigned d) const noexcept { return begin_[d].idx; }
      const_reference at(unsigned d) const { return begin_[d].idx; }

    private:
      /// implementation detail
      index_view(index_pointer b, index_pointer e) : begin_(b), end_(e) {}

      index_pointer begin_, end_;
      friend class accessor;
    };

    // assignment is pass-through
    accessor& operator=(const accessor& o) {
      get() = o.get();
      return *this;
    }

    // assignment is pass-through
    template <class T>
    accessor& operator=(const T& x) {
      get() = x;
      return *this;
    }

    /// Returns the cell reference.
    value_reference get() const noexcept { return *(iter_.iter_); }
    /// @copydoc get()
    value_reference operator*() const noexcept { return get(); }
    /// Access fields and methods of the cell object.
    value_iterator operator->() const noexcept { return iter_.iter_; }

    /// Access current index.
    /// @param d axis dimension.
    axis::index_type index(unsigned d = 0) const noexcept {
      return iter_.indices_[d].idx;
    }

    /// Access indices as an iterable range.
    index_view indices() const noexcept {
      BOOST_ASSERT(iter_.indices_.hist_);
      return {iter_.indices_.data(),
              iter_.indices_.data() + iter_.indices_.hist_->rank()};
    }

    /// Access current bin.
    /// @tparam N axis dimension.
    template <unsigned N = 0>
    decltype(auto) bin(std::integral_constant<unsigned, N> = {}) const {
      BOOST_ASSERT(iter_.indices_.hist_);
      return iter_.indices_.hist_->axis(std::integral_constant<unsigned, N>())
          .bin(index(N));
    }

    /// Access current bin.
    /// @param d axis dimension.
    decltype(auto) bin(unsigned d) const {
      BOOST_ASSERT(iter_.indices_.hist_);
      return iter_.indices_.hist_->axis(d).bin(index(d));
    }

    /** Computes density in current cell.

      The density is computed as the cell value divided by the product of bin widths. Axes
      without bin widths, like axis::category, are treated as having unit bin with.
    */
    double density() const {
      BOOST_ASSERT(iter_.indices_.hist_);
      double x = 1;
      unsigned d = 0;
      iter_.indices_.hist_->for_each_axis([&](const auto& a) {
        const auto w = axis::traits::width_as<double>(a, this->index(d++));
        x *= w ? w : 1;
      });
      return get() / x;
    }

    // forward all comparison operators to the value
    bool operator<(const accessor& o) noexcept { return get() < o.get(); }
    bool operator>(const accessor& o) noexcept { return get() > o.get(); }
    bool operator==(const accessor& o) noexcept { return get() == o.get(); }
    bool operator!=(const accessor& o) noexcept { return get() != o.get(); }
    bool operator<=(const accessor& o) noexcept { return get() <= o.get(); }
    bool operator>=(const accessor& o) noexcept { return get() >= o.get(); }

    template <class U>
    bool operator<(const U& o) const noexcept {
      return get() < o;
    }

    template <class U>
    bool operator>(const U& o) const noexcept {
      return get() > o;
    }

    template <class U>
    bool operator==(const U& o) const noexcept {
      return get() == o;
    }

    template <class U>
    bool operator!=(const U& o) const noexcept {
      return get() != o;
    }

    template <class U>
    bool operator<=(const U& o) const noexcept {
      return get() <= o;
    }

    template <class U>
    bool operator>=(const U& o) const noexcept {
      return get() >= o;
    }

    operator value_type() const noexcept { return get(); }

  private:
    accessor(iterator& i) noexcept : iter_(i) {}

    accessor(const accessor&) = default; // only callable by indexed_range::iterator

    iterator& iter_;

    friend class iterator;
  };

  /// implementation detail
  class iterator {
  public:
    using value_type = typename indexed_range::value_type;
    using reference = accessor;

  private:
    struct pointer_proxy {
      reference* operator->() noexcept { return std::addressof(ref_); }
      reference ref_;
    };

  public:
    using pointer = pointer_proxy;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;

    reference operator*() noexcept { return *this; }
    pointer operator->() noexcept { return pointer_proxy{operator*()}; }

    iterator& operator++() {
      BOOST_ASSERT(indices_.hist_);
      std::size_t stride = 1;
      auto c = indices_.begin();
      ++c->idx;
      ++iter_;
      while (c->idx == c->end && (c != (indices_.end() - 1))) {
        c->idx = c->begin;
        iter_ -= (c->end - c->begin) * stride;
        stride *= c->extent;
        ++c;
        ++c->idx;
        iter_ += stride;
      }
      return *this;
    }

    iterator operator++(int) {
      auto prev = *this;
      operator++();
      return prev;
    }

    bool operator==(const iterator& x) const noexcept { return iter_ == x.iter_; }
    bool operator!=(const iterator& x) const noexcept { return !operator==(x); }

  private:
    iterator(histogram_type* h) : iter_(h->begin()), indices_(h) {}

    value_iterator iter_;

    struct index_data {
      axis::index_type idx, begin, end, extent;
    };

    struct indices_t : std::array<index_data, buffer_size> {
      using base_type = std::array<index_data, buffer_size>;

      indices_t(histogram_type* h) noexcept : hist_{h} {}

      constexpr auto end() noexcept { return base_type::begin() + hist_->rank(); }
      constexpr auto end() const noexcept { return base_type::begin() + hist_->rank(); }
      histogram_type* hist_;
    } indices_;

    friend class indexed_range;
  };

  indexed_range(histogram_type& hist, coverage cov) : begin_(&hist), end_(&hist) {
    std::size_t stride = 1;
    auto ca = begin_.indices_.begin();
    const auto clast = ca + begin_.indices_.hist_->rank() - 1;
    begin_.indices_.hist_->for_each_axis(
        [ca, clast, cov, &stride, this](const auto& a) mutable {
          using opt = axis::traits::static_options<decltype(a)>;
          constexpr int under = opt::test(axis::option::underflow);
          constexpr int over = opt::test(axis::option::overflow);
          const auto size = a.size();

          ca->extent = size + under + over;
          // -1 if underflow and cover all, else 0
          ca->begin = cov == coverage::all ? -under : 0;
          // size + 1 if overflow and cover all, else size
          ca->end = cov == coverage::all ? size + over : size;
          ca->idx = ca->begin;

          begin_.iter_ += (ca->begin + under) * stride;
          end_.iter_ += ((ca < clast ? ca->begin : ca->end) + under) * stride;

          stride *= ca->extent;
          ++ca;
        });
  }

  iterator begin() noexcept { return begin_; }
  iterator end() noexcept { return end_; }

private:
  iterator begin_, end_;
};

/** Generates an indexed range of <a
  href="https://en.cppreference.com/w/cpp/named_req/ForwardIterator">forward iterators</a>
  over the histogram cells.

  Use this in a range-based for loop:

  ```
  for (auto&& x : indexed(hist)) { ... }
  ```

  This generates an optimized loop which is nearly always faster than a hand-written loop
  over the histogram cells. The iterators dereference to an indexed_range::accessor, which
  has methods to query the current indices and bins and acts like a pointer to the cell
  value. The returned iterators are forward iterators. They can be stored in a container,
  but may not be used after the life-time of the histogram ends.

  @returns indexed_range

  @param hist Reference to the histogram.
  @param cov  Iterate over all or only inner bins (optional, default: inner).
 */
template <class Histogram>
auto indexed(Histogram&& hist, coverage cov = coverage::inner) {
  return indexed_range<std::remove_reference_t<Histogram>>{std::forward<Histogram>(hist),
                                                           cov};
}

} // namespace histogram
} // namespace boost

#endif
