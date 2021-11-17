// Copyright (C) 2019 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/stl_interfaces/iterator_interface.hpp>

#include <algorithm>
#include <array>
#include <vector>

#include <cassert>


//[ filtered_int_iterator_defn
template<typename Pred>
struct filtered_int_iterator : boost::stl_interfaces::iterator_interface<
                                   filtered_int_iterator<Pred>,
                                   std::bidirectional_iterator_tag,
                                   int>
{
    filtered_int_iterator() : it_(nullptr) {}
    filtered_int_iterator(int * it, int * last, Pred pred) :
        it_(it),
        last_(last),
        pred_(std::move(pred))
    {
        // We need to do this in the constructor so that operator== works
        // properly on two filtered_int_iterators, when they bound a sequence
        // in which none of the ints meets the predicate.
        it_ = std::find_if(it_, last_, pred_);
    }

    // A bidirectional iterator based on iterator_interface usually required
    // four user-defined operations.  since we are adapting an existing
    // iterator (an int *), we only need to define this one.  The others are
    // implemented by iterator_interface, using the underlying int *.
    filtered_int_iterator & operator++()
    {
        it_ = std::find_if(std::next(it_), last_, pred_);
        return *this;
    }

    // It is really common for iterator adaptors to have a base() member
    // function that returns the adapted iterator.
    int * base() const { return it_; }

private:
    // Provide access to these private members.
    friend boost::stl_interfaces::access;

    // These functions are picked up by iterator_interface, and used to
    // implement any operations that you don't define above.  They're not
    // called base() so that they do not collide with the base() member above.
    //
    // Note that the const overload does not strictly speaking need to be a
    // reference, as demonstrated here.
    constexpr int *& base_reference() noexcept { return it_; }
    constexpr int * base_reference() const noexcept { return it_; }

    int * it_;
    int * last_;
    Pred pred_;
};

// A make-function makes it easier to deal with the Pred parameter.
template<typename Pred>
auto make_filtered_int_iterator(int * it, int * last, Pred pred)
{
    return filtered_int_iterator<Pred>(it, last, std::move(pred));
}
//]


int main()
{
    //[ filtered_int_iterator_usage
    std::array<int, 8> ints = {{0, 1, 2, 3, 4, 5, 6, 7}};
    int * const ints_first = ints.data();
    int * const ints_last = ints.data() + ints.size();

    auto even = [](int x) { return (x % 2) == 0; };
    auto first = make_filtered_int_iterator(ints_first, ints_last, even);
    auto last = make_filtered_int_iterator(ints_last, ints_last, even);

    // This is an example only.  Obviously, we could have called
    // std::copy_if() here.
    std::vector<int> ints_copy;
    std::copy(first, last, std::back_inserter(ints_copy));
    assert(ints_copy == (std::vector<int>{0, 2, 4, 6}));
    //]
}
