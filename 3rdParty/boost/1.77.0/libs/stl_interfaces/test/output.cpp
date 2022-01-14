// Copyright (C) 2019 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/stl_interfaces/iterator_interface.hpp>

#include <boost/core/lightweight_test.hpp>

#include <array>
#include <numeric>
#include <vector>
#include <type_traits>


struct basic_output_iter
    : boost::stl_interfaces::
          iterator_interface<basic_output_iter, std::output_iterator_tag, int>
{
    basic_output_iter() : it_(nullptr) {}
    basic_output_iter(int * it) : it_(it) {}

    int & operator*() noexcept { return *it_; }
    basic_output_iter & operator++() noexcept
    {
        ++it_;
        return *this;
    }

    using base_type = boost::stl_interfaces::
        iterator_interface<basic_output_iter, std::output_iterator_tag, int>;
    using base_type::operator++;

private:
    int * it_;
};

using output = basic_output_iter;

#if 201703L < __cplusplus && defined(__cpp_lib_concepts)
static_assert(std::output_iterator<output, int>, "");
#endif
BOOST_STL_INTERFACES_STATIC_ASSERT_ITERATOR_TRAITS(
    output,
    std::output_iterator_tag,
    std::output_iterator_tag,
    int,
    int &,
    void,
    std::ptrdiff_t)

template<typename Container>
struct back_insert_iter : boost::stl_interfaces::iterator_interface<
                              back_insert_iter<Container>,
                              std::output_iterator_tag,
                              typename Container::value_type,
                              back_insert_iter<Container> &>
{
    back_insert_iter() : c_(nullptr) {}
    back_insert_iter(Container & c) : c_(std::addressof(c)) {}

    back_insert_iter & operator*() noexcept { return *this; }
    back_insert_iter & operator++() noexcept { return *this; }

    back_insert_iter & operator=(typename Container::value_type const & v)
    {
        c_->push_back(v);
        return *this;
    }
    back_insert_iter & operator=(typename Container::value_type && v)
    {
        c_->push_back(std::move(v));
        return *this;
    }

    using base_type = boost::stl_interfaces::iterator_interface<
        back_insert_iter<Container>,
        std::output_iterator_tag,
        typename Container::value_type,
        back_insert_iter<Container> &>;
    using base_type::operator++;

private:
    Container * c_;
};

using back_insert = back_insert_iter<std::vector<int>>;

#if 201703L < __cplusplus && defined(__cpp_lib_concepts)
static_assert(std::output_iterator<back_insert, int>, "");
#endif
BOOST_STL_INTERFACES_STATIC_ASSERT_ITERATOR_TRAITS(
    back_insert,
    std::output_iterator_tag,
    std::output_iterator_tag,
    int,
    back_insert &,
    void,
    std::ptrdiff_t)


std::vector<int> ints = {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}};


int main()
{

{
    std::vector<int> ints_copy(ints.size());
    std::copy(ints.begin(), ints.end(), output(&ints_copy[0]));
    BOOST_TEST(ints_copy == ints);
}


{
    std::vector<int> ints_copy;
    std::copy(ints.begin(), ints.end(), back_insert(ints_copy));
    BOOST_TEST(ints_copy == ints);
}


{
    std::vector<int> ints_copy;
    back_insert out(ints_copy);
    for (int i = 0; i < 10; ++i)
        out++;
}

    return boost::report_errors();
}
