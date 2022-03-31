# stl_interfaces

An updated C++20-friendly version of the `iterator_facade` and
`iterator_adaptor` parts of Boost.Iterator (now called `iterator_interface`);
a pre-C++20 version of C++20's `view_interface`; and a new template called
`container_interface`, meant to aid the creation of new containers; all
targeting standardization.  This library requires at least C++14.

For the iterator portion -- if you need to write an iterator, `iterator_interface` turns this:

```c++
    struct repeated_chars_iterator
    {
        using value_type = char;
        using difference_type = std::ptrdiff_t;
        using pointer = char const *;
        using reference = char const;
        using iterator_category = std::random_access_iterator_tag;

        constexpr repeated_chars_iterator() noexcept :
            first_(nullptr),
            size_(0),
            n_(0)
        {}
        constexpr repeated_chars_iterator(
            char const * first,
            difference_type size,
            difference_type n) noexcept :
            first_(first),
            size_(size),
            n_(n)
        {}

        constexpr reference operator*() const noexcept
        {
            return first_[n_ % size_];
        }

        constexpr value_type operator[](difference_type n) const noexcept
        {
            return first_[(n_ + n) % size_];
        }

        constexpr repeated_chars_iterator & operator++() noexcept
        {
            ++n_;
            return *this;
        }
        constexpr repeated_chars_iterator operator++(int)noexcept
        {
            repeated_chars_iterator retval = *this;
            ++*this;
            return retval;
        }
        constexpr repeated_chars_iterator & operator+=(difference_type n) noexcept
        {
            n_ += n;
            return *this;
        }

        constexpr repeated_chars_iterator & operator--() noexcept
        {
            --n_;
            return *this;
        }
        constexpr repeated_chars_iterator operator--(int)noexcept
        {
            repeated_chars_iterator retval = *this;
            --*this;
            return retval;
        }
        constexpr repeated_chars_iterator & operator-=(difference_type n) noexcept
        {
            n_ -= n;
            return *this;
        }

        friend constexpr bool operator==(
            repeated_chars_iterator lhs, repeated_chars_iterator rhs) noexcept
        {
            return lhs.first_ == rhs.first_ && lhs.n_ == rhs.n_;
        }
        friend constexpr bool operator!=(
            repeated_chars_iterator lhs, repeated_chars_iterator rhs) noexcept
        {
            return !(lhs == rhs);
        }
        friend constexpr bool operator<(
            repeated_chars_iterator lhs, repeated_chars_iterator rhs) noexcept
        {
            return lhs.first_ == rhs.first_ && lhs.n_ < rhs.n_;
        }
        friend constexpr bool operator<=(
            repeated_chars_iterator lhs, repeated_chars_iterator rhs) noexcept
        {
            return lhs == rhs || lhs < rhs;
        }
        friend constexpr bool operator>(
            repeated_chars_iterator lhs, repeated_chars_iterator rhs) noexcept
        {
            return rhs < lhs;
        }
        friend constexpr bool operator>=(
            repeated_chars_iterator lhs, repeated_chars_iterator rhs) noexcept
        {
            return rhs <= lhs;
        }

        friend constexpr repeated_chars_iterator
        operator+(repeated_chars_iterator lhs, difference_type rhs) noexcept
        {
            return lhs += rhs;
        }
        friend constexpr repeated_chars_iterator
        operator+(difference_type lhs, repeated_chars_iterator rhs) noexcept
        {
            return rhs += lhs;
        }
        friend constexpr repeated_chars_iterator
        operator-(repeated_chars_iterator lhs, difference_type rhs) noexcept
        {
            return lhs -= rhs;
        }
        friend constexpr difference_type operator-(
            repeated_chars_iterator lhs, repeated_chars_iterator rhs) noexcept
        {
            return lhs.n_ - rhs.n_;
        }

    private:
        char const * first_;
        difference_type size_;
        difference_type n_;
    };
```

into this:

```c++
struct repeated_chars_iterator : boost::stl_interfaces::iterator_interface<
                                     repeated_chars_iterator,
                                     std::random_access_iterator_tag,
                                     char,
                                     char>
{
    constexpr repeated_chars_iterator() noexcept :
        first_(nullptr),
        size_(0),
        n_(0)
    {}
    constexpr repeated_chars_iterator(
        char const * first, difference_type size, difference_type n) noexcept :
        first_(first),
        size_(size),
        n_(n)
    {}

    constexpr char operator*() const noexcept { return first_[n_ % size_]; }
    constexpr repeated_chars_iterator & operator+=(std::ptrdiff_t i) noexcept
    {
        n_ += i;
        return *this;
    }
    constexpr auto operator-(repeated_chars_iterator other) const noexcept
    {
        return n_ - other.n_;
    }

private:
    char const * first_;
    difference_type size_;
    difference_type n_;
};
```

The code size savings are even more dramatic for `view_interface` and
`container_interface`! If you don't ever write iterators, range views, or
containers, this is not for you.

Online docs: https://boostorg.github.io/stl_interfaces.

This library includes a temporary implementation for those who wish to experiment with
a concept-constrained version before C++20 is widely implemented.  Casey Carter's cmcstl2
is an implementation of the `std::ranges` portion of the C++20 standard library.  To use it:

- check out the cmcstl2 branch of this library; then

- put its headers in your include path, so that they can be included with
  `#include <stl2/foo.hpp>`; and

- build with GCC 8 or 9, including the `-fconcepts` and `-std=c++2a` flags.

GCC 8 and 9 are the only compilers with an adequate concepts implementation at
the time of this writing.


[![Build Status](https://travis-ci.org/boostorg/stl_interfaces.svg?branch=develop)](https://travis-ci.org/boostorg/stl_interfaces)
[![Build Status](https://ci.appveyor.com/api/projects/status/github/tzlaine/stl-interfaces?branch=develop&svg=true)](https://ci.appveyor.com/project/tzlaine/stl-interfaces)
[![License](https://img.shields.io/badge/license-boost-brightgreen.svg)](LICENSE_1_0.txt)
