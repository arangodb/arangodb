// Copyright (C) 2019 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//[ back_insert_iterator
#include <boost/stl_interfaces/iterator_interface.hpp>

#include <algorithm>
#include <vector>

#include <cassert>


// This is an output iterator that holds a reference to a container, and calls
// push_back() on that container when it is written to, just like
// std::back_insert_iterator.  This is not a lot less code to write than the
// implementation of std::back_insert_iterator, but it demonstrates that
// iterator_interface is flexible enough to handle this odd kind of iterator.
template<typename Container>
struct back_insert_iterator : boost::stl_interfaces::iterator_interface<
                                  back_insert_iterator<Container>,
                                  std::output_iterator_tag,
                                  typename Container::value_type,
                                  back_insert_iterator<Container> &>
{
    // For concept requirements.
    back_insert_iterator() : c_(nullptr) {}

    // Construct from a container.
    explicit back_insert_iterator(Container & c) : c_(std::addressof(c)) {}

    // When writing to *this, copy v into the container via push_back().
    back_insert_iterator & operator=(typename Container::value_type const & v)
    {
        c_->push_back(v);
        return *this;
    }
    // When writing to *this, move v into the container via push_back().
    back_insert_iterator & operator=(typename Container::value_type && v)
    {
        c_->push_back(std::move(v));
        return *this;
    }

    // Dereferencing *this just returns a reference to *this, so that the
    // expression *it = value uses the operator=() overloads above.
    back_insert_iterator & operator*() { return *this; }
    // There is no underlying sequence over which we are iterating, so there's
    // nowhere to go in next().  Do nothing.
    back_insert_iterator & operator++() { return *this; }

    using base_type = boost::stl_interfaces::iterator_interface<
        back_insert_iterator<Container>,
        std::output_iterator_tag,
        typename Container::value_type,
        back_insert_iterator<Container> &>;
    using base_type::operator++;

private:
    Container * c_;
};

// A convenience function that creates a back_insert_iterator<Container>.
template<typename Container>
back_insert_iterator<Container> back_inserter(Container & c)
{
    return back_insert_iterator<Container>(c);
}


int main()
{
    std::vector<int> ints = {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}};
    std::vector<int> ints_copy;
    std::copy(ints.begin(), ints.end(), ::back_inserter(ints_copy));
    assert(ints_copy == ints);
}
//]
