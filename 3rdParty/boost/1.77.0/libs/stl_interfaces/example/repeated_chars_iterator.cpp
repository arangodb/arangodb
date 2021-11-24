// Copyright (C) 2019 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/stl_interfaces/iterator_interface.hpp>

#include <string>

#include <cassert>


//[ repeated_chars_iterator
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
//]


int main()
{
    //[ repeated_chars_iterator_usage
    repeated_chars_iterator first("foo", 3, 0); // 3 is the length of "foo", 0 is this iterator's position.
    repeated_chars_iterator last("foo", 3, 7);  // Same as above, but now the iterator's position is 7.
    std::string result;
    std::copy(first, last, std::back_inserter(result));
    assert(result == "foofoof");
    //]
}
