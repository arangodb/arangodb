// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_SEGMENTED_VECTOR_HPP
#define BOOST_TEXT_SEGMENTED_VECTOR_HPP

#include <boost/text/segmented_vector_fwd.hpp>
#include <boost/text/detail/btree.hpp>
#include <boost/text/detail/iterator.hpp>
#include <boost/text/detail/make_container.hpp>
#include <boost/text/detail/vector_iterator.hpp>

#include <boost/stl_interfaces/sequence_container_interface.hpp>

#include <initializer_list>


namespace boost { namespace text {

    namespace detail {
        constexpr int seg_insert_max = 512;
    }

    /** A sequence container of `T` with discontiguous storage.  Insertion and
        erasure are efficient at any position within the sequence, and copies
        of the entire container are extremely cheap.  The elements are
        immutable.  In order to "mutate" one, use the single-element overload
        of `replace()`. */
    template<typename T, typename Segment>
#if BOOST_TEXT_USE_CONCEPTS
    // clang-format off
        requires std::is_same_v<T, std::ranges::range_value_t<Segment>>
#endif
    struct segmented_vector
        // clang-format on
        : boost::stl_interfaces::sequence_container_interface<
              segmented_vector<T, Segment>,
              boost::stl_interfaces::element_layout::discontiguous>
    {
        using value_type = T;
        using pointer = T *;
        using const_pointer = T const *;
        using reference = value_type const &;
        using const_reference = reference;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using iterator = detail::const_vector_iterator<T, Segment>;
        using const_iterator = iterator;
        using reverse_iterator = stl_interfaces::reverse_iterator<iterator>;
        using const_reverse_iterator = reverse_iterator;
        using segment_type = Segment;

        /** Default ctor.

            \post `size() == 0 && begin() == end()` */
        segmented_vector() noexcept : ptr_()
        {}

        explicit segmented_vector(size_type n) : ptr_() { resize(n); }
        explicit segmented_vector(size_type n, T const & x) : ptr_()
        {
            resize(n, x);
        }

        template<typename Iter, typename Sentinel>
        segmented_vector(Iter first, Sentinel last)
        {
            assign(first, last);
        }
        segmented_vector(std::initializer_list<T> il) :
            segmented_vector(il.begin(), il.end())
        {}
        segmented_vector(segmented_vector const &) = default;
        segmented_vector(segmented_vector &&) noexcept = default;
        segmented_vector & operator=(segmented_vector const &) = default;
        segmented_vector & operator=(segmented_vector &&) noexcept = default;
        ~segmented_vector() {}

        const_iterator begin() noexcept { return const_iterator(*this, 0); }
        const_iterator end() noexcept { return const_iterator(*this, size()); }

        size_type size() const noexcept { return detail::size(ptr_.get()); }
        size_type max_size() const noexcept { return PTRDIFF_MAX; }
        void resize(size_type sz) noexcept
        {
            if (sz < size()) {
                erase(begin() + sz, end());
            } else {
                ptr_ = detail::btree_insert(
                    ptr_, size(), detail::make_node(segment_type(size() - sz)));
            }
        }
        void resize(size_type sz, T const & x) noexcept
        {
            if (sz < size()) {
                erase(begin() + sz, end());
            } else {
                ptr_ = detail::btree_insert(
                    ptr_,
                    size(),
                    detail::make_node(segment_type(size() - sz, x)));
            }
        }

        template<typename Iter, typename Sentinel>
        void assign(Iter first, Sentinel last)
        {
            auto seg = detail::make_container<segment_type>(first, last);
            replace(begin(), end(), std::move(seg));
        }
        template<typename Iter>
        void assign(size_type n, T const & x)
        {
            replace(begin(), end(), segment_type(n, x));
        }

        template<typename... Args>
        const_reference emplace_front(Args &&... args)
        {
            return *emplace(begin(), (Args &&) args...);
        }
        template<typename... Args>
        const_reference emplace_back(Args &&... args)
        {
            return *emplace(end(), (Args &&) args...);
        }
        template<typename... Args>
        const_iterator emplace(const_iterator at, Args &&... args)
        {
            BOOST_ASSERT(begin() <= at && at <= end());

            int const offset = at - begin();

            if (seg_insertion insertion =
                    mutable_insertion_leaf(at, 1, would_allocate)) {
                detail::bump_along_path_to_leaf(ptr_, at - begin(), 1);
                insertion.seg_->insert(
                    insertion.seg_->begin() + insertion.found_.offset_,
                    T((Args &&) args...));
            } else {
                ptr_ = detail::btree_insert(
                    ptr_,
                    offset,
                    detail::make_node(segment_type(1, T((Args &&) args...))));
            }

            return begin() + offset;
        }
        template<typename Iter, typename Sentinel>
        const_iterator insert(const_iterator at, Iter first, Sentinel last)
        {
            BOOST_ASSERT(begin() <= at && at <= end());

            if (first == last)
                return at;

            int const offset = at - begin();

            segment_type seg;
            for (; first != last; ++first) {
                seg.push_back(*first);
            }

            ptr_ = detail::btree_insert(
                ptr_, offset, detail::make_node(std::move(seg)));

            return begin() + offset;
        }
        const_iterator insert(const_iterator at, segment_type v)
        {
            BOOST_ASSERT(begin() <= at && at <= end());

            if (v.empty())
                return at;

            int const offset = at - begin();

            if (seg_insertion insertion =
                    mutable_insertion_leaf(at, v.size(), would_not_allocate)) {
                auto const v_size = v.size();
                detail::bump_along_path_to_leaf(ptr_, at - begin(), v_size);
                insertion.seg_->insert(
                    insertion.seg_->begin() + insertion.found_.offset_,
                    v.begin(),
                    v.end());
            } else {
                ptr_ = detail::btree_insert(
                    ptr_, at - begin(), detail::make_node(std::move(v)));
            }

            return begin() + offset;
        }
        const_iterator erase(const_iterator first, const_iterator last)
        {
            BOOST_ASSERT(first <= last);
            BOOST_ASSERT(begin() <= first && last <= end());

            if (first == last)
                return first;

            int const offset = first - begin();

            auto const lo = first - begin();
            auto const hi = last - begin();
            auto const size = hi - lo;
            if (seg_insertion insertion =
                    mutable_insertion_leaf(first, -size, would_not_allocate)) {
                detail::bump_along_path_to_leaf(ptr_, lo, -size);
                insertion.seg_->erase(
                    insertion.seg_->begin() + insertion.found_.offset_,
                    insertion.seg_->begin() + insertion.found_.offset_ + size);
            } else {
                ptr_ = btree_erase(ptr_, lo, hi);
            }

            return begin() + offset;
        }

        void swap(segmented_vector & other) { ptr_.swap(other.ptr_); }

        using base_type = boost::stl_interfaces::sequence_container_interface<
            segmented_vector<T, Segment>,
            boost::stl_interfaces::element_layout::discontiguous>;
        using base_type::begin;
        using base_type::end;
        using base_type::insert;
        using base_type::erase;

        /** Returns true if `*this` and `other` contain the same root node
            pointer.  This is useful when you want to check for equality
            between two `segmented_vector`s that are likely to have originated
            from the same initial `segmented_vector`, and may have since been
            mutated. */
        bool equal_root(segmented_vector other) const noexcept
        {
            return ptr_ == other.ptr_;
        }

        /** Replaces the element at position `at` with `t`, by moving `t`.

            If `*this` hold the only reference to the underlying data, this is
            more efficient than calling `erase()` in combination with
            `insert()`.

            \pre `begin() <= old_substr.begin() && old_substr.end() <= end()` */
        segmented_vector & replace(const_iterator at, T t)
        {
            BOOST_ASSERT(begin() <= at && at <= end());

            if (seg_insertion insertion =
                    mutable_insertion_leaf(at, 0, would_allocate)) {
                (*insertion.seg_)[insertion.found_.offset_] = std::move(t);
            } else {
                auto const offset = at - begin();
                ptr_ = detail::btree_erase(ptr_, offset, offset + 1);
                ptr_ = detail::btree_insert(
                    ptr_,
                    offset,
                    detail::make_node(segment_type(1, std::move(t))));
            }

            return *this;
        }

        /** Replaces the portion of `*this` delimited by `[first, last)` with
            the sequence of `T` from `v` by moving the contents of `v`.

            \pre `begin() <= old_substr.begin() && old_substr.end() <= end()` */
        segmented_vector &
        replace(const_iterator first, const_iterator last, segment_type v)
        {
            auto const it = erase(first, last);
            insert(it, std::move(v));
            return *this;
        }

        /** Replaces the portion of `*this` delimited by `[old_first,
            old_last)` with the `T` sequence `[new_first, new_last)`.

           \pre `begin() <= old_substr.begin() && old_substr.end() <= end()` */
        template<typename Iter, typename Sentinel>
        segmented_vector & replace(
            const_iterator old_first,
            const_iterator old_last,
            Iter new_first,
            Sentinel new_last)
        {
            auto seg =
                detail::make_container<segment_type>(new_first, new_last);
            auto const it = erase(old_first, old_last);
            insert(it, std::move(seg));
            return *this;
        }

        /** Visits each segment s of *this and calls f(s).  Each segment is a
            string_view.  Depending of the operation performed on each
            segment, this may be more efficient than iterating over [begin(),
            end()).

            \pre Fn is an invocable accepting an iterator and a sentinel. */
        template<typename Fn>
        void foreach_segment(Fn && f) const
        {
            detail::foreach_leaf(
                ptr_, [&](detail::leaf_node_t<T, Segment> const * leaf) {
                    switch (leaf->which_) {
                    case detail::leaf_node_t<T, Segment>::which::seg:
                        f(leaf->as_seg().begin(), leaf->as_seg().end());
                        break;
                    case detail::leaf_node_t<T, Segment>::which::ref: {
                        auto const & ref = leaf->as_reference();
                        auto const leaf_begin =
                            ref.seg_.as_leaf()->as_seg().begin();
                        f(leaf_begin + ref.lo_, leaf_begin + ref.hi_);
                        break;
                    }
                    default: BOOST_ASSERT(!"unhandled rope node case"); break;
                    }
                    return true;
                });
        }

#ifdef BOOST_TEXT_TESTING
        friend void dump_tree(std::ostream & os, segmented_vector const & s)
        {
            if (s.empty())
                os << "[EMPTY]\n";
            else
                detail::dump_tree(os, s.ptr_);
        }
#endif

        friend void swap(segmented_vector & lhs, segmented_vector & rhs)
        {
            lhs.swap(rhs);
        }

#ifndef BOOST_TEXT_DOXYGEN

    private:
        enum allocation_note_t { would_allocate, would_not_allocate };

        struct seg_insertion
        {
            explicit operator bool() const { return seg_ != nullptr; }

            segment_type * seg_;
            detail::found_leaf<T, Segment> found_;
        };

        seg_insertion mutable_insertion_leaf(
            const_iterator at,
            size_type delta,
            allocation_note_t allocation_note)
        {
            if (!ptr_)
                return seg_insertion{nullptr};

            detail::found_leaf<T, Segment> found;
            if (0 < delta && at == end()) {
                detail::find_leaf(ptr_, at - begin() - 1, found);
                ++found.offset_;
            } else {
                detail::find_leaf(ptr_, at - begin(), found);
            }

            for (auto node : found.path_) {
                if (1 < node->refs_)
                    return seg_insertion{nullptr};
            }

            if (1 < found.leaf_->get()->refs_)
                return seg_insertion{nullptr};

            if (found.leaf_->as_leaf()->which_ ==
                detail::leaf_node_t<T, Segment>::which::seg) {
                segment_type & v = const_cast<segment_type &>(
                    found.leaf_->as_leaf()->as_seg());
                auto const inserted_size = v.size() + delta;
                if (delta < 0 && v.size() < found.offset_ + -delta) {
                    return seg_insertion{nullptr};
                }
                if ((0 < inserted_size && inserted_size <= v.capacity()) ||
                    (allocation_note == would_allocate &&
                     inserted_size <= detail::seg_insert_max)) {
                    return seg_insertion{&v, found};
                }
            }

            return seg_insertion{nullptr};
        }

        detail::node_ptr<T, Segment> ptr_;

        friend struct detail::const_vector_iterator<T, Segment>;

#endif
    };

}}

#endif
