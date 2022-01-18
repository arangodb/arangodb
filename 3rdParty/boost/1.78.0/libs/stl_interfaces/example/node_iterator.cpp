// Copyright (C) 2019 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/stl_interfaces/iterator_interface.hpp>

#include <algorithm>
#include <array>
#include <iostream>

#include <cassert>


//[ node_defn
template<typename T>
struct node
{
    T value_;
    node * next_; // == nullptr in the tail node
};
//]

//[ node_iterator_class_head
template<typename T>
struct node_iterator
    : boost::stl_interfaces::
          iterator_interface<node_iterator<T>, std::forward_iterator_tag, T>
//]
{
    //[ node_iterator_ctors
    constexpr node_iterator() noexcept : it_(nullptr) {}
    constexpr node_iterator(node<T> * it) noexcept : it_(it) {}
    //]

    //[ node_iterator_user_ops
    constexpr T & operator*() const noexcept { return it_->value_; }
    constexpr node_iterator & operator++() noexcept
    {
        it_ = it_->next_;
        return *this;
    }
    friend constexpr bool
    operator==(node_iterator lhs, node_iterator rhs) noexcept
    {
        return lhs.it_ == rhs.it_;
    }
    //]

    //[ node_iterator_using_declaration
    using base_type = boost::stl_interfaces::
        iterator_interface<node_iterator<T>, std::forward_iterator_tag, T>;
    using base_type::operator++;
    //]

private:
    node<T> * it_;
};

//[ node_iterator_concept_check Equivalent to
// static_assert(std::forward_iterator<node_iterator>, ""), or nothing in
// C++17 and earlier.
BOOST_STL_INTERFACES_STATIC_ASSERT_CONCEPT(
    node_iterator<int>, std::forward_iterator)
//]


int main()
{
    std::array<node<int>, 5> nodes;
    std::generate(nodes.begin(), nodes.end(), [] {
        static int i = 0;
        return node<int>{i++};
    });
    std::adjacent_find(
        nodes.begin(), nodes.end(), [](node<int> & a, node<int> & b) {
            a.next_ = &b;
            return false;
        });
    nodes.back().next_ = nullptr;

    //[ node_iterator_usage
    node_iterator<int> const first(&nodes[0]);
    node_iterator<int> const last;
    for (auto it = first; it != last; it++) {
        std::cout << *it << " "; // Prints 0 1 2 3 4
    }
    std::cout << "\n";
    //]
}
