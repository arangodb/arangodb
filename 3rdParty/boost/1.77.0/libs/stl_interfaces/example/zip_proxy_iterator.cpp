// Copyright (C) 2019 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//[ zip_proxy_iterator
#include <boost/stl_interfaces/iterator_interface.hpp>

#include <algorithm>
#include <array>
#include <tuple>

#include <cassert>


// This is a zip iterator, meaning that it iterates over a notional sequence
// of pairs that is formed from two actual sequences of scalars.  To make this
// iterator writable, it needs to have a reference type that is not actually a
// reference -- the reference type is a pair of references, std::tuple<int &,
// int &>.
struct zip_iterator : boost::stl_interfaces::proxy_iterator_interface<
                      zip_iterator,
                      std::random_access_iterator_tag,
                      std::tuple<int, int>,
                      std::tuple<int &, int &>>
{
    constexpr zip_iterator() noexcept : it1_(), it2_() {}
    constexpr zip_iterator(int * it1, int * it2) noexcept : it1_(it1), it2_(it2)
    {}

    constexpr std::tuple<int &, int &> operator*() const noexcept
    {
        return std::tuple<int &, int &>{*it1_, *it2_};
    }
    constexpr zip_iterator & operator += (std::ptrdiff_t i) noexcept
    {
        it1_ += i;
        it2_ += i;
        return *this;
    }
    constexpr auto operator-(zip_iterator other) const noexcept
    {
        return it1_ - other.it1_;
    }

private:
    int * it1_;
    int * it2_;
};


namespace std {
    // Required for std::sort to work with zip_iterator.  Without this
    // overload, std::sort eventually tries to call std::swap(*it1, *it2),
    // *it1 and *it2 are rvalues, and std::swap() takes mutable lvalue
    // references.  That makes std::swap(*it1, *it2) ill-formed.
    //
    // Note that this overload does not conflict with any other swap()
    // overloads, since this one takes rvalue reference parameters.
    //
    // Also note that this overload has to be in namespace std only because
    // ADL cannot find it anywhere else.  If
    // zip_iterator::reference/std::tuple's template parameters were not
    // builtins, this overload could be in whatever namespace those template
    // parameters were declared in.
    void swap(zip_iterator::reference && lhs, zip_iterator::reference && rhs)
    {
        using std::swap;
        swap(std::get<0>(lhs), std::get<0>(rhs));
        swap(std::get<1>(lhs), std::get<1>(rhs));
    }
}


int main()
{
    std::array<int, 10> ints = {{2, 0, 1, 5, 3, 6, 8, 4, 9, 7}};
    std::array<int, 10> ones = {{1, 1, 1, 1, 1, 1, 1, 1, 1, 1}};

    {
        std::array<std::tuple<int, int>, 10> const result = {{
            {2, 1},
            {0, 1},
            {1, 1},
            {5, 1},
            {3, 1},
            {6, 1},
            {8, 1},
            {4, 1},
            {9, 1},
            {7, 1},
        }};

        zip_iterator first(ints.data(), ones.data());
        zip_iterator last(ints.data() + ints.size(), ones.data() + ones.size());
        assert(std::equal(first, last, result.begin(), result.end()));
    }

    {
        std::array<std::tuple<int, int>, 10> const result = {{
            {0, 1},
            {1, 1},
            {2, 1},
            {3, 1},
            {4, 1},
            {5, 1},
            {6, 1},
            {7, 1},
            {8, 1},
            {9, 1},
        }};
        zip_iterator first(ints.data(), ones.data());
        zip_iterator last(ints.data() + ints.size(), ones.data() + ones.size());
        assert(!std::equal(first, last, result.begin(), result.end()));
        std::sort(first, last);
        assert(std::equal(first, last, result.begin(), result.end()));
    }
}
//]
