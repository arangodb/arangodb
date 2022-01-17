// Copyright (C) 2019 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//[ random_access_iterator
#include <boost/stl_interfaces/iterator_interface.hpp>

#include <algorithm>
#include <array>

#include <cassert>


// This is a minimal random access iterator.  It uses default template
// parameters for most stl_interfaces template parameters.
struct simple_random_access_iterator
    : boost::stl_interfaces::iterator_interface<
          simple_random_access_iterator,
          std::random_access_iterator_tag,
          int>
{
    // This default constructor does not initialize it_, since that's how int *
    // works as well.  This allows optimum performance in code paths where
    // initializing a single pointer may be measurable.  It is also a
    // reasonable choice to initialize with nullptr.
    simple_random_access_iterator() noexcept {}
    simple_random_access_iterator(int * it) noexcept : it_(it) {}

    int & operator*() const noexcept { return *it_; }
    simple_random_access_iterator & operator+=(std::ptrdiff_t i) noexcept
    {
        it_ += i;
        return *this;
    }
    auto operator-(simple_random_access_iterator other) const noexcept
    {
        return it_ - other.it_;
    }

private:
    int * it_;
};


int main()
{
    std::array<int, 10> ints = {{0, 2, 1, 3, 4, 5, 7, 6, 8, 9}};

    simple_random_access_iterator first(ints.data());
    simple_random_access_iterator last(ints.data() + ints.size());
    std::sort(first, last);
    assert(ints == (std::array<int, 10>{{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}}));
}
//]
