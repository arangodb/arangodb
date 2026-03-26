// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_DETAIL_VECTOR_ITERATOR_ITERATOR_HPP
#define BOOST_TEXT_DETAIL_VECTOR_ITERATOR_ITERATOR_HPP

#include <boost/text/normalize_fwd.hpp>
#include <boost/text/segmented_vector_fwd.hpp>
#include <boost/text/rope_fwd.hpp>

#include <boost/stl_interfaces/iterator_interface.hpp>


namespace boost { namespace text { namespace detail {

    template<typename T, typename Segment>
    struct const_vector_iterator : stl_interfaces::iterator_interface<
                                       const_vector_iterator<T, Segment>,
                                       std::random_access_iterator_tag,
                                       T,
                                       T const &,
                                       T const *>
    {
        const_vector_iterator() noexcept :
            vec_(nullptr), n_(-1), leaf_(nullptr), leaf_start_(-1)
        {}

        const_vector_iterator(
            segmented_vector<T, Segment> const & v, std::ptrdiff_t n) noexcept :
            vec_(&v), n_(n), leaf_(nullptr), leaf_start_(0)
        {}

        T const & operator*() const noexcept
        {
            if (leaf_) {
                return deref();
            } else {
                found_element<T, Segment> found;
                find_element(vec_->ptr_, n_, found);
                leaf_ = found.leaf_.leaf_->as_leaf();
                leaf_start_ = n_ - found.leaf_.offset_;
                return *found.element_;
            }
        }

        const_vector_iterator & operator+=(std::ptrdiff_t n) noexcept
        {
            n_ += n;
            leaf_ = nullptr;
            return *this;
        }

        friend std::ptrdiff_t
        operator-(const_vector_iterator lhs, const_vector_iterator rhs) noexcept
        {
            BOOST_ASSERT(lhs.vec_ == rhs.vec_);
            return lhs.n_ - rhs.n_;
        }

        using segment_type =
            typename segmented_vector<T, Segment>::segment_type;

        const_vector_iterator(
            segmented_vector<T, Segment> const * v, std::ptrdiff_t n) noexcept :
            vec_(v), n_(n), leaf_(nullptr), leaf_start_(0)
        {}

        T const & deref() const
        {
            switch (leaf_->which_) {
            default: BOOST_ASSERT(!"unhandled leaf node case");
            case leaf_node_t<T, Segment>::which::seg: {
                segment_type const * v =
                    static_cast<segment_type *>(leaf_->buf_ptr_);
                return *(v->begin() + (n_ - leaf_start_));
            }
            case leaf_node_t<T, Segment>::which::ref: {
                detail::reference<T, Segment> const * ref =
                    static_cast<detail::reference<T, Segment> *>(
                        leaf_->buf_ptr_);
                return *(
                    ref->seg_.as_leaf()->as_seg().begin() + ref->lo_ +
                    (n_ - leaf_start_));
            }
            }
        }

        segmented_vector<T, Segment> const * vec_;
        std::ptrdiff_t n_;
        mutable leaf_node_t<T, Segment> const * leaf_;
        mutable std::ptrdiff_t leaf_start_;
    };

}}}

#endif
