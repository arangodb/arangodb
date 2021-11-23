// Copyright (C) 2021 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_STL_INTERFACES_USE_CONCEPTS

void compile_seq_cont_constrained_pop_back() {}

#else

#include <initializer_list>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/stl_interfaces/sequence_container_interface.hpp>

template<class T>
struct container : boost::stl_interfaces::sequence_container_interface<
                       container<T>,
                       boost::stl_interfaces::element_layout::contiguous>
{
public:
    using super = boost::stl_interfaces::sequence_container_interface<
        container<T>,
        boost::stl_interfaces::element_layout::contiguous>;
    using container_type = std::vector<T>;

    using value_type = typename container_type::value_type;
    using reference = typename container_type::reference;
    using const_reference = typename container_type::const_reference;
    using iterator = typename container_type::iterator;
    using const_iterator = typename container_type::const_iterator;
    using difference_type = typename container_type::difference_type;
    using size_type = typename container_type::size_type;

    container() = default;
    container(const container &) = default;
    container(container &&) = default;
    ~container() = default;
    container & operator=(const container &) = default;
    container & operator=(container &&) = default;

    container(size_type count, const_reference value) : c(count, value) {}
    container(std::initializer_list<value_type> init) : c(init) {}

    template<class InputIt>
    container(InputIt first, InputIt last) : c(first, last)
    {}

    iterator begin() noexcept { return c.begin(); }
    iterator end() noexcept { return c.end(); }

    size_type max_size() const noexcept { return c.max_size(); }

    template<class InputIt>
    iterator insert(const_iterator pos, InputIt first, InputIt last)
    {
        return c.insert(pos, first, last);
    }

    template<class... Args>
    iterator emplace(const_iterator pos, Args &&... args)
    {
        return c.emplace(pos, std::forward<Args>(args)...);
    }

    iterator erase(const_iterator first, const_iterator last)
    {
        return c.erase(first, last);
    }

    template<class... Args>
    requires(std::constructible_from<value_type, Args &&...>) reference
        emplace_back(Args &&... args)
    {
        return c.emplace_back(std::forward<Args>(args)...);
    }

    void swap(container & other) { c.swap(other.c); }

    using super::begin;
    using super::end;
    using super::insert;
    using super::erase;

private:
    container_type c;
};

void compile_seq_cont_constrained_pop_back()
{
    {
        std::vector<int> v;
        v.emplace_back(0);
        v.pop_back();

        container<int> c;
        c.emplace_back(0);
        c.pop_back();
    }

    {
        std::vector<std::unique_ptr<int>> v;
        v.emplace_back(new int);
        v.pop_back();

        container<std::unique_ptr<int>> c;
        c.emplace_back(new int);
        c.pop_back();
    }
}

#endif
