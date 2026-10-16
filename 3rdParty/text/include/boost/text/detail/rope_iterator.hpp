// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_DETAIL_ROPE_ITERATOR_HPP
#define BOOST_TEXT_DETAIL_ROPE_ITERATOR_HPP

#include <boost/text/detail/vector_iterator.hpp>


namespace boost { namespace text { namespace detail {

    template<typename Char, typename String>
    struct const_rope_view_iterator
        : stl_interfaces::iterator_interface<
              const_rope_view_iterator<Char, String>,
              std::random_access_iterator_tag,
              Char,
              Char>
    {
        using const_rope_iterator = const_vector_iterator<Char, String>;

        const_rope_view_iterator() noexcept : which_(which::r) {}
        explicit const_rope_view_iterator(const_rope_iterator it) noexcept :
            r_(it), which_(which::r)
        {}
        explicit const_rope_view_iterator(Char const * it) noexcept :
            tv_(it), which_(which::tv)
        {}

        const_rope_iterator as_rope_iter() const
        {
            BOOST_ASSERT(which_ == which::r);
            return r_;
        }

        Char operator*() const noexcept
        {
            switch (which_) {
            case which::r: return *r_;
            case which::tv: return *tv_;
            }
            return '\0'; // This should never execute.
        }

        const_rope_view_iterator & operator+=(std::ptrdiff_t n) noexcept
        {
            switch (which_) {
            case which::r: r_ += n; break;
            case which::tv: tv_ += n; break;
            }
            return *this;
        }

        friend std::ptrdiff_t operator-(
            const_rope_view_iterator lhs, const_rope_view_iterator rhs) noexcept
        {
            BOOST_ASSERT(lhs.which_ == rhs.which_);
            switch (lhs.which_) {
            case which::r: return lhs.r_ - rhs.r_;
            case which::tv: return lhs.tv_ - rhs.tv_;
            }
            return 0; // This should never execute.
        }

    private:
        enum class which { r, tv };

        const_rope_iterator r_;
        Char const * tv_;

        which which_;
    };

}}}

#endif
