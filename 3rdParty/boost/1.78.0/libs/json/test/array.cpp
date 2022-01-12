//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

// Test that header file is self-contained.
#include <boost/json/array.hpp>

#include <boost/json/monotonic_resource.hpp>

#include <forward_list>
#include <list>
#include <iterator>

#include "test.hpp"
#include "test_suite.hpp"

BOOST_JSON_NS_BEGIN

BOOST_STATIC_ASSERT( std::is_nothrow_destructible<array>::value );
BOOST_STATIC_ASSERT( std::is_nothrow_move_constructible<array>::value );

class array_test
{
public:
    using init_list =
        std::initializer_list<value_ref>;

    string_view const str_;

    array_test()
        : str_(
            "abcdefghijklmnopqrstuvwxyz")
    {
        // ensure this string does
        // not fit in the SBO area.
        BOOST_ASSERT(str_.size() >
            string().capacity());

    }

    void
    check(array const& a)
    {
        BOOST_TEST(a.size() == 3);
        BOOST_TEST(a[0].is_number());
        BOOST_TEST(a[1].is_bool());
        BOOST_TEST(a[2].is_string());
    }

    void
    check(
        array const& a,
        storage_ptr const& sp)
    {
        check(a);
        check_storage(a, sp);
    }

    void
    testDestroy()
    {
        {
            monotonic_resource mr;
            array a(&mr);
            a.reserve(3);
        }
        {
            monotonic_resource mr;
            array a(&mr);
            a.resize(3);
            a.clear();
        }
    }

    void
    testCtors()
    {
        // ~array()
        {
            // implied
        }

        // array()
        {
            array a;
            BOOST_TEST(a.empty());
            BOOST_TEST(a.size() == 0);
        }

        // array(storage_ptr)
        {
            array a(storage_ptr{});
            check_storage(a, storage_ptr{});
        }

        // array(size_type, value, storage)
        {
            // default storage
            {
                array a(3, true);
                BOOST_TEST(a.size() == 3);
                for(auto const& v : a)
                    BOOST_TEST(v.is_bool());
                check_storage(a, storage_ptr{});
            }

            // construct with zero `true` values
            {
                array(0, true);
            }

            // construct with three `true` values
            fail_loop([&](storage_ptr const& sp)
            {
                array a(3, true, sp);
                BOOST_TEST(a.size() == 3);
                check_storage(a, sp);
            });
        }

        // array(size_type, storage)
        {
            // default storage
            {
                array a(3);
                BOOST_TEST(a.size() == 3);
                for(auto const& v : a)
                    BOOST_TEST(v.is_null());
                check_storage(a, storage_ptr{});
            }

            // zero size
            {
                array a(0);
                BOOST_TEST(a.empty());
            }

            fail_loop([&](storage_ptr const& sp)
            {
                array a(3, sp);
                BOOST_TEST(a.size() == 3);
                for(auto const& v : a)
                    BOOST_TEST(v.is_null());
                check_storage(a, sp);
            });
        }

        // array(InputIt, InputIt, storage)
        {
            // empty range
            {
                // random-access iterator
                std::vector<value> i1;
                array a1(i1.begin(), i1.end());
                BOOST_TEST(a1.empty());

                // bidirectional iterator
                std::list<value> i2;
                array a2(i2.begin(), i2.end());
                BOOST_TEST(a2.empty());

                // forward iterator
                std::forward_list<value> i3;
                array a3(i3.begin(), i3.end());
                BOOST_TEST(a3.empty());

                // input iterator
                auto const it = make_input_iterator(i3.begin());
                array a4(it, it);
                BOOST_TEST(a4.empty());
            }

            // default storage
            {
                init_list init{ 0, 1, str_, 3, 4 };
                array a(init.begin(), init.end());
                check_storage(a, storage_ptr{});
                BOOST_TEST(a[0].as_int64() == 0);
                BOOST_TEST(a[1].as_int64() == 1);
                BOOST_TEST(a[2].as_string() == str_);
                BOOST_TEST(a[3].as_int64() == 3);
                BOOST_TEST(a[4].as_int64() == 4);
            }

            // random iterator
            fail_loop([&](storage_ptr const& sp)
            {
                init_list init{ 1, true, str_ };
                array a(init.begin(), init.end(), sp);
                check(a);
                check_storage(a, sp);
            });

            // input iterator
            fail_loop([&](storage_ptr const& sp)
            {
                init_list init{ 1, true, str_ };
                array a(
                    make_input_iterator(init.begin()),
                    make_input_iterator(init.end()), sp);
                check(a);
                check_storage(a, sp);
            });
        }

        // array(array const&)
        {
            {
                array a1;
                array a2(a1);
            }

            {
                array a1;
                array a2({ 1, true, str_ });
                a2 = a1;
            }

            {
                init_list init{ 1, true, str_ };
                array a1(init.begin(), init.end());
                array a2(a1);
                check(a2);
                check_storage(a2, storage_ptr{});
            }
        }

        // array(array const&, storage)
        fail_loop([&](storage_ptr const& sp)
        {
            init_list init{ 1, true, str_ };
            array a1(init.begin(), init.end());
            array a2(a1, sp);
            check(a2);
            check_storage(a2, sp);
        });

        // array(pilfered<array>)
        {
            {
                init_list init{ 1, true, str_ };
                array a1(init.begin(), init.end());
                array a2(pilfer(a1));
                BOOST_TEST(a1.empty());
                check(a2);
                check_storage(a2, storage_ptr{});
            }

            // ensure pilfered-from objects
            // are trivially destructible
            {
                array a1(make_shared_resource<
                    monotonic_resource>());
                array a2(pilfer(a1));
                BOOST_TEST(a1.storage().get() ==
                    storage_ptr().get());
            }
        }

        // array(array&&)
        {
            init_list init{ 1, true, str_ };
            array a1(init.begin(), init.end());
            array a2 = std::move(a1);
            BOOST_TEST(a1.empty());
            check(a2);
            check_storage(a2, storage_ptr{});
        }

        // array(array&&, storage)
        {
            {
                init_list init{ 1, true, str_ };
                array a1(init.begin(), init.end());
                array a2(
                    std::move(a1), storage_ptr{});
                BOOST_TEST(a1.empty());
                check(a2);
                check_storage(a1, storage_ptr{});
                check_storage(a2, storage_ptr{});
            }

            fail_loop([&](storage_ptr const& sp)
            {
                init_list init{ 1, true, str_ };
                array a1(init.begin(), init.end());
                array a2(std::move(a1), sp);
                BOOST_TEST(! a1.empty());
                check(a2);
                check_storage(a1, storage_ptr{});
                check_storage(a2, sp);
            });
        }

        // array(init_list, storage)
        {
            // default storage
            {
                array a({1, true, str_});
                check(a);
                check_storage(a, storage_ptr{});
            }

            fail_loop([&](storage_ptr const& sp)
            {
                array a({1, true, str_}, sp);
                check(a, sp);
                check_storage(a, sp);
            });
        }
    }

    void
    testAssign()
    {
        // operator=(array const&)
        {
            {
                array a1({1, true, str_});
                array a2({nullptr, object{}, 1.f});
                a2 = a1;
                check(a1);
                check(a2);
                check_storage(a1, storage_ptr{});
                check_storage(a2, storage_ptr{});
            }

            fail_loop([&](storage_ptr const& sp)
            {
                array a1({1, true, str_});
                array a2({nullptr, object{}, 1.f}, sp);
                a2 = a1;
                check(a1);
                check(a2);
                check_storage(a1, storage_ptr{});
                check_storage(a2, sp);
            });

            // self-assign
            {
                array a({1, true, str_});
                auto& a1 = a;
                auto& a2 = a;
                a1 = a2;
                check(a);
            }

            // copy from child
            {
                array a({1, {1,2,3}, 3});
                a = a.at(1).as_array();
                BOOST_TEST(
                    a == array({1, 2, 3}));
            }
        }

        // operator=(array&&)
        {
            {
                array a1({1, true, str_});
                array a2({nullptr, object{}, 1.f});
                a2 = std::move(a1);
                BOOST_TEST(a1.empty());
                check(a2);
            }

            // empty
            {
                array a1;
                array a2;
                a2 = std::move(a1);
                BOOST_TEST(a1.empty());
                BOOST_TEST(a2.empty());
            }

            fail_loop([&](storage_ptr const& sp)
            {
                array a1({1, true, str_});
                array a2({nullptr, object{}, 1.f}, sp);
                a2 = std::move(a1);
                check(a1);
                check(a2);
                check_storage(a1, storage_ptr{});
                check_storage(a2, sp);
            });

            // self-move
            {
                array a({1, true, str_});
                auto& a1 = a;
                auto& a2 = a;
                a1 = std::move(a2);
                check(a);
            }

            // move from child
            {
                array a({1, {1,2,3}, 3});
                a = std::move(a.at(1).as_array());
                BOOST_TEST(
                    a == array({1, 2, 3}));
            }
        }

        // operator=(init_list)
        {
            {
                array a;
                a = {};
            }

            {
                array a({ 1, true, str_ });
                a = {};
            }

            {
                init_list init{ 1, true, str_ };
                array a({nullptr, object{}, 1.f});
                a = init;
                check(a);
                check_storage(a, storage_ptr{});
            }

            fail_loop([&](storage_ptr const& sp)
            {
                init_list init{ 1, true, str_ };
                array a({nullptr, object{}, 1.f}, sp);
                a = init;
                check(a);
                check_storage(a, sp);
            });
        }
    }

    void
    testGetStorage()
    {
        // storage()
        {
            // implied
        }
    }

    void
    testAccess()
    {
        // at(pos)
        {
            array a({1, true, str_});
            BOOST_TEST(a.at(0).is_number());
            BOOST_TEST(a.at(1).is_bool());
            BOOST_TEST(a.at(2).is_string());
            try
            {
                a.at(3);
                BOOST_TEST_FAIL();
            }
            catch(std::out_of_range const&)
            {
                BOOST_TEST_PASS();
            }
        }

        // at(pos) const
        {
            array const a({1, true, str_});
            BOOST_TEST(a.at(0).is_number());
            BOOST_TEST(a.at(1).is_bool());
            BOOST_TEST(a.at(2).is_string());
            try
            {
                a.at(3);
                BOOST_TEST_FAIL();
            }
            catch(std::out_of_range const&)
            {
                BOOST_TEST_PASS();
            }
        }

        // operator[&](size_type)
        {
            array a({1, true, str_});
            BOOST_TEST(a[0].is_number());
            BOOST_TEST(a[1].is_bool());
            BOOST_TEST(a[2].is_string());
        }

        // operator[&](size_type) const
        {
            array const a({1, true, str_});
            BOOST_TEST(a[0].is_number());
            BOOST_TEST(a[1].is_bool());
            BOOST_TEST(a[2].is_string());
        }

        // front()
        {
            array a({1, true, str_});
            BOOST_TEST(a.front().is_number());
        }

        // front() const
        {
            array const a({1, true, str_});
            BOOST_TEST(a.front().is_number());
        }

        // back()
        {
            array a({1, true, str_});
            BOOST_TEST(a.back().is_string());
        }

        // back() const
        {
            array const a({1, true, str_});
            BOOST_TEST(a.back().is_string());
        }

        // if_contains()
        // if_contains() const
        {
            {
                array a({1, true, str_});
                BOOST_TEST(a.if_contains(1)->is_bool());
                BOOST_TEST(a.if_contains(3) == nullptr);
            }
            {
                array const a({1, true, str_});
                BOOST_TEST(a.if_contains(1)->is_bool());
                BOOST_TEST(a.if_contains(3) == nullptr);
            }
        }

        // data()
        {
            {
                array a({1, true, str_});
                BOOST_TEST(a.data() == &a[0]);
            }
            {
                array a;
                BOOST_TEST(
                    a.data() == a.begin());
            }
        }

        // data() const
        {
            {
                array const a({1, true, str_});
                BOOST_TEST(a.data() == &a[0]);
            }
            {
                array const a{};
                BOOST_TEST(
                    a.data() == a.begin());
            }
        }
    }

    void
    testIterators()
    {
        array a({1, true, str_});
        auto const& ac(a);

        {
            auto it = a.begin();
            BOOST_TEST(it->is_number()); ++it;
            BOOST_TEST(it->is_bool());   it++;
            BOOST_TEST(it->is_string()); ++it;
            BOOST_TEST(it == a.end());
        }
        {
            auto it = a.cbegin();
            BOOST_TEST(it->is_number()); ++it;
            BOOST_TEST(it->is_bool());   it++;
            BOOST_TEST(it->is_string()); ++it;
            BOOST_TEST(it == a.cend());
        }
        {
            auto it = ac.begin();
            BOOST_TEST(it->is_number()); ++it;
            BOOST_TEST(it->is_bool());   it++;
            BOOST_TEST(it->is_string()); ++it;
            BOOST_TEST(it == ac.end());
        }
        {
            auto it = a.end();
            --it; BOOST_TEST(it->is_string());
            it--; BOOST_TEST(it->is_bool());
            --it; BOOST_TEST(it->is_number());
            BOOST_TEST(it == a.begin());
        }
        {
            auto it = a.cend();
            --it; BOOST_TEST(it->is_string());
            it--; BOOST_TEST(it->is_bool());
            --it; BOOST_TEST(it->is_number());
            BOOST_TEST(it == a.cbegin());
        }
        {
            auto it = ac.end();
            --it; BOOST_TEST(it->is_string());
            it--; BOOST_TEST(it->is_bool());
            --it; BOOST_TEST(it->is_number());
            BOOST_TEST(it == ac.begin());
        }

        {
            auto it = a.rbegin();
            BOOST_TEST(it->is_string()); ++it;
            BOOST_TEST(it->is_bool());   it++;
            BOOST_TEST(it->is_number()); ++it;
            BOOST_TEST(it == a.rend());
        }
        {
            auto it = a.crbegin();
            BOOST_TEST(it->is_string()); ++it;
            BOOST_TEST(it->is_bool());   it++;
            BOOST_TEST(it->is_number()); ++it;
            BOOST_TEST(it == a.crend());
        }
        {
            auto it = ac.rbegin();
            BOOST_TEST(it->is_string()); ++it;
            BOOST_TEST(it->is_bool());   it++;
            BOOST_TEST(it->is_number()); ++it;
            BOOST_TEST(it == ac.rend());
        }
        {
            auto it = a.rend();
            --it; BOOST_TEST(it->is_number());
            it--; BOOST_TEST(it->is_bool());
            --it; BOOST_TEST(it->is_string());
            BOOST_TEST(it == a.rbegin());
        }
        {
            auto it = a.crend();
            --it; BOOST_TEST(it->is_number());
            it--; BOOST_TEST(it->is_bool());
            --it; BOOST_TEST(it->is_string());
            BOOST_TEST(it == a.crbegin());
        }
        {
            auto it = ac.rend();
            --it; BOOST_TEST(it->is_number());
            it--; BOOST_TEST(it->is_bool());
            --it; BOOST_TEST(it->is_string());
            BOOST_TEST(it == ac.rbegin());
        }

        {
            array a2;
            array const& ca2(a2);
            BOOST_TEST(std::distance(
                a2.begin(), a2.end()) == 0);
            BOOST_TEST(std::distance(
                ca2.begin(), ca2.end()) == 0);
            BOOST_TEST(std::distance(
                ca2.cbegin(), ca2.cend()) == 0);
            BOOST_TEST(std::distance(
                a2.rbegin(), a2.rend()) == 0);
            BOOST_TEST(std::distance(
                ca2.rbegin(), ca2.rend()) == 0);
            BOOST_TEST(std::distance(
                ca2.crbegin(), ca2.crend()) == 0);
        }
    }

    void
    testCapacity()
    {
        // empty()
        {
            {
                array a;
                BOOST_TEST(a.empty());
                a.emplace_back(1);
                BOOST_TEST(! a.empty());
            }

            {
                array a({1, 2});
                BOOST_TEST(! a.empty());
                a.clear();
                BOOST_TEST(a.empty());
                BOOST_TEST(a.capacity() > 0);
            }
        }

        // size()
        {
            array a;
            BOOST_TEST(a.size() == 0);
            a.emplace_back(1);
            BOOST_TEST(a.size() == 1);
        }

        // max_size()
        {
            array a;
            BOOST_TEST(a.max_size() > 0);
        }

        // reserve()
        {
            {
                array a;
                a.reserve(0);
            }

            {
                array a(3);
                a.reserve(1);
            }

            {
                array a(3);
                a.reserve(0);
            }

            {
                array a;
                a.reserve(50);
                BOOST_TEST(a.capacity() >= 50);
            }

            fail_loop([&](storage_ptr const& sp)
            {
                array a(4, 'c', sp);
                a.reserve(a.capacity() + 1);
                auto const new_cap = a.capacity();
                // 1.5x growth
                BOOST_TEST(new_cap == 6);
                a.reserve(new_cap + 1);
                BOOST_TEST(a.capacity() == 9);
            });
        }

        // capacity()
        {
            array a;
            BOOST_TEST(a.capacity() == 0);
        }

        // shrink_to_fit()
        {
            {
                array a(1);
                a.shrink_to_fit();
                BOOST_TEST(a.size() == 1);
                BOOST_TEST(a.capacity() >= 1);
            }

            {
                array a(2, 'c');
                BOOST_TEST(a.capacity() == 2);
                a.erase(a.begin());
                a.shrink_to_fit();
                BOOST_TEST(a.capacity() == 1);
            }

            fail_loop([&](storage_ptr const& sp)
            {
                array a(1, sp);
                a.resize(a.capacity());
                a.shrink_to_fit();
                BOOST_TEST(a.size() == a.capacity());
            });

            fail_loop([&](storage_ptr const& sp)
            {
                array a(sp);
                a.reserve(10);
                BOOST_TEST(a.capacity() >= 10);
                a.shrink_to_fit();
                BOOST_TEST(a.capacity() == 0);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                array a(4, sp);
                a.reserve(a.capacity() * 2);
                BOOST_TEST(a.capacity() >= 4);
                a.shrink_to_fit();
                if(a.capacity() != a.size())
                    throw test_failure{};
            });
        }
    }

    void
    testModifiers()
    {
        // clear
        {
            {
                array a;
                BOOST_TEST(a.size() == 0);
                BOOST_TEST(a.capacity() == 0);
                a.clear();
                BOOST_TEST(a.size() == 0);
                BOOST_TEST(a.capacity() == 0);
            }
            {
                array a({1, true, str_});
                a.clear();
                BOOST_TEST(a.size() == 0);
                BOOST_TEST(a.capacity() > 0);
            }
        }

        // insert(const_iterator, value_type const&)
        {
            // fast path
            fail_loop([&](storage_ptr const& sp)
            {
                array a({1, 2, 3}, sp);
                a.pop_back();
                BOOST_TEST(
                    a.capacity() > a.size());
                a.insert(a.begin(), 1);
                BOOST_TEST(
                    a == array({1, 1, 2}));
            });

            // self-insert
            {
                array a = {1, 2, 3};
                a.insert(a.begin(), a[1]);
                BOOST_TEST(
                    a == array({2, 1, 2, 3}));
            }

            // fail
            fail_loop([&](storage_ptr const& sp)
            {
                array a({1, str_}, sp);
                value v(true);
                a.insert(a.begin() + 1, v);
                check(a);
                check_storage(a, sp);
            });
        }

        // insert(const_iterator, value_type&&)
        fail_loop([&](storage_ptr const& sp)
        {
            array a({1, str_}, sp);
            value v(true);
            a.insert(
                a.begin() + 1, std::move(v));
            check(a);
            check_storage(a, sp);
        });

        // insert(const_iterator, size_type, value_type const&)
        {
            // fast path
            fail_loop([&](storage_ptr const& sp)
            {
                array a({1, str_}, sp);
                a.reserve(3);
                BOOST_TEST(
                    a.capacity() > a.size());
                a.insert(&a[1], 1, true);
                check(a);
            });

            // zero
            {
                array a(3);
                a.insert(&a[1], 0, true);
                BOOST_TEST(a.size() == 3);
            }

            fail_loop([&](storage_ptr const& sp)
            {
                value v({1,2,3});
                array a({1, str_}, sp);
                a.insert(a.begin() + 1, 3, v);
                BOOST_TEST(a[0].is_number());
                BOOST_TEST(a[1].as_array().size() == 3);
                BOOST_TEST(a[2].as_array().size() == 3);
                BOOST_TEST(a[3].as_array().size() == 3);
                BOOST_TEST(a[4].is_string());
            });
        }

        // insert(const_iterator, InputIt, InputIt)
        {
            // random iterator
            fail_loop([&](storage_ptr const& sp)
            {
                std::initializer_list<
                    value> init = {1, true};
                array a({str_}, sp);
                a.insert(a.begin(),
                    init.begin(), init.end());
                check(a);
            });

            // random iterator (multiple growth)
            fail_loop([&](storage_ptr const& sp)
            {
                std::initializer_list<value_ref> init = {
                     1, str_, true,  1,  2,  3,  4,  5,  6,
                     7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
                    17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
                    27, 28, 29, 30 };
                array a(sp);
                a.insert(a.begin(),
                    init.begin(), init.end());
                BOOST_TEST(a.size() == init.size());
                BOOST_TEST(a.capacity() == init.size());
            });

            // input iterator (empty range)
            {
                std::initializer_list<value_ref> init;
                array a;
                a.insert(a.begin(),
                    make_input_iterator(init.begin()),
                    make_input_iterator(init.end()));
                BOOST_TEST(a.empty());
            }

            // input iterator
            fail_loop([&](storage_ptr const& sp)
            {
                std::initializer_list<
                    value> init = {1, true};
                array a({str_}, sp);
                a.insert(a.begin(),
                    make_input_iterator(init.begin()),
                    make_input_iterator(init.end()));
                check(a);
            });

            // input iterator (multiple growth)
            fail_loop([&](storage_ptr const& sp)
            {
                std::initializer_list<value_ref> init =
                    {1, true, 1, 2, 3, 4, 5, 6, 7};
                array a({str_}, sp);
                a.insert(a.begin(),
                    make_input_iterator(init.begin()),
                    make_input_iterator(init.end()));
                BOOST_TEST(a.size() == init.size() + 1);
            });

            // backward relocate
            fail_loop([&](storage_ptr const& sp)
            {
                std::initializer_list<value_ref> init = {1, 2};
                array a({"a", "b", "c", "d", "e"}, sp);
                a.insert(
                    a.begin() + 1,
                    init.begin(), init.end());
            });
        }

        // insert(const_iterator, init_list)
        fail_loop([&](storage_ptr const& sp)
        {
            array a({0, 3, 4}, sp);
            auto it = a.insert(
                a.begin() + 1, {1, str_});
            BOOST_TEST(it == a.begin() + 1);
            BOOST_TEST(a[0].as_int64() == 0);
            BOOST_TEST(a[1].as_int64() == 1);
            BOOST_TEST(a[2].as_string() == str_);
            BOOST_TEST(a[3].as_int64() == 3);
            BOOST_TEST(a[4].as_int64() == 4);
        });

        // emplace(const_iterator, arg)
        fail_loop([&](storage_ptr const& sp)
        {
            array a({0, 2, 3, 4}, sp);
            auto it = a.emplace(
                a.begin() + 1, str_);
            BOOST_TEST(it == a.begin() + 1);
            BOOST_TEST(a[0].as_int64() == 0);
            BOOST_TEST(a[1].as_string() == str_);
            BOOST_TEST(a[2].as_int64() == 2);
            BOOST_TEST(a[3].as_int64() == 3);
            BOOST_TEST(a[4].as_int64() == 4);
        });

        // erase(pos)
        {
            array a({1, true, nullptr, str_});
            a.erase(a.begin() + 2);
            check(a);
        }

        // erase(first, last)
        {
            array a({1, true, nullptr, 1.f, str_});
            a.erase(
                a.begin() + 2,
                a.begin() + 4);
            check(a);
        }

        // push_back(value const&)
        {
            // fast path
            {
                array a = {1, 2, 3};
                a.pop_back();
                BOOST_TEST(
                    a.capacity() > a.size());
                a.push_back(1);
                BOOST_TEST(a ==
                    array({1, 2, 1}));
            }

            // self push_back
            {
                array a = {1, 2, 3};
                a.push_back(a[1]);
                BOOST_TEST(a ==
                    array({1, 2, 3, 2}));
            }

            fail_loop([&](storage_ptr const& sp)
            {
                array a({1, true}, sp);
                value v(str_);
                a.push_back(v);
                BOOST_TEST(
                    v.as_string() == str_);
                check(a);
                check_storage(a, sp);
            });
        }

        // push_back(value&&)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                array a({1, true}, sp);
                value v(str_);
                a.push_back(std::move(v));
                check(a);
                check_storage(a, sp);
            });
        }

        // emplace_back(arg)
        fail_loop([&](storage_ptr const& sp)
        {
            array a({1, true}, sp);
            a.emplace_back(str_);
            check(a);
            check_storage(a, sp);
        });

        // pop_back()
        fail_loop([&](storage_ptr const& sp)
        {
            array a({1, true, str_, nullptr}, sp);
            a.pop_back();
            check(a);
            check_storage(a, sp);
        });

        // resize(size_type)
        {
            value v(array{});
            v.as_array().emplace_back(1);
            v.as_array().emplace_back(true);
            v.as_array().emplace_back(str_);

            fail_loop([&](storage_ptr const& sp)
            {
                array a(5, sp);
                a.resize(3);
                BOOST_TEST(a.size() == 3);
                check_storage(a, sp);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                array a(sp);
                a.resize(3);
                BOOST_TEST(a.size() == 3);
                check_storage(a, sp);
            });
        }

        // resize(size_type, value_type const&)
        {
            value v(array{});
            v.as_array().emplace_back(1);
            v.as_array().emplace_back(true);
            v.as_array().emplace_back(str_);

            fail_loop([&](storage_ptr const& sp)
            {
                array a(5, v, sp);
                a.resize(3, v);
                BOOST_TEST(a.size() == 3);
                check_storage(a, sp);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                array a(3, v, sp);
                a.resize(5, v);
                BOOST_TEST(a.size() == 5);
                check_storage(a, sp);
            });
        }

        // swap
        {
            // same storage
            {
                array a1({1, true, str_});
                array a2 = {1.};
                a1.swap(a2);
                check(a2);
                BOOST_TEST(a1.size() == 1);
                BOOST_TEST(a1.front().is_number());
                BOOST_TEST(a1.front().as_double() == 1.);
            }

            // different storage
            fail_loop([&](storage_ptr const& sp)
            {
                array a1({1, true, str_}, sp);
                array a2 = {1.};
                a1.swap(a2);
                check(a2);
                BOOST_TEST(a1.size() == 1);
            });
            fail_loop([&](storage_ptr const& sp)
            {
                array a1 = {1.};
                array a2({1, true, str_}, sp);
                a1.swap(a2);
                check(a1);
                BOOST_TEST(a2.size() == 1);
            });
        }
    }

    void
    testExceptions()
    {
        // operator=(array const&)
        fail_loop([&](storage_ptr const& sp)
        {
            array a0({1, true, str_});
            array a1;
            array a(sp);
            a.emplace_back(nullptr);
            a = a0;
            a1 = a;
            check(a1);
        });

        // operator=(init_list)
        fail_loop([&](storage_ptr const& sp)
        {
            init_list init{ 1, true, str_ };
            array a1;
            array a(sp);
            a.emplace_back(nullptr);
            a = init;
            a1 = a;
            check(a1);
        });

        // insert(const_iterator, count, value_type const&)
        fail_loop([&](storage_ptr const& sp)
        {
            array a1;
            array a({1, true}, sp);
            a.insert(a.begin() + 1,
                3, nullptr);
            a1 = a;
            BOOST_TEST(a1.size() == 5);
            BOOST_TEST(a1[0].is_number());
            BOOST_TEST(a1[1].is_null());
            BOOST_TEST(a1[2].is_null());
            BOOST_TEST(a1[3].is_null());
            BOOST_TEST(a1[4].is_bool());
        });

        // insert(const_iterator, InputIt, InputIt)
        fail_loop([&](storage_ptr const& sp)
        {
            init_list init{ 1, true, str_ };
            array a1;
            array a(sp);
            a.insert(a.end(),
                init.begin(), init.end());
            a1 = a;
            check(a1);
        });

        // emplace(const_iterator, arg)
        fail_loop([&](storage_ptr const& sp)
        {
            array a1;
            array a({1, nullptr}, sp);
            a.emplace(a.begin() + 1, true);
            a1 = a;
            BOOST_TEST(a1.size() == 3);
            BOOST_TEST(a1[0].is_number());
            BOOST_TEST(a1[1].is_bool());
            BOOST_TEST(a1[2].is_null());
        });

        // emplace(const_iterator, arg)
        fail_loop([&](storage_ptr const& sp)
        {
            array a1;
            array a({1, str_}, sp);
            a.emplace(a.begin() + 1, true);
            a1 = a;
            check(a1);
            BOOST_TEST(a1.size() == 3);
            BOOST_TEST(a1[0].is_number());
            BOOST_TEST(a1[1].is_bool());
            BOOST_TEST(a1[2].is_string());
        });
    }

    void
    testEquality()
    {
        BOOST_TEST(array({}) == array({}));
        BOOST_TEST(array({}) != array({1,2}));
        BOOST_TEST(array({1,2,3}) == array({1,2,3}));
        BOOST_TEST(array({1,2,3}) != array({1,2}));
        BOOST_TEST(array({1,2,3}) != array({3,2,1}));
    }

    void
    testHash()
    {
        BOOST_TEST(check_hash_equal(
            array{}, array()));
        BOOST_TEST(check_hash_equal(
            array{1,1}, array{1,1}));
        BOOST_TEST(check_hash_equal(
            array{1,1}, array{1,1}));
        BOOST_TEST(expect_hash_not_equal(
            array{1,1}, array{1,2}));
        BOOST_TEST(check_hash_equal(
            array{"a", "b", 17},
            array{"a", "b", 17U}));
        BOOST_TEST(expect_hash_not_equal(
            array{"a", "b", nullptr},
            array{nullptr, "a", "b"}));
    }

    void
    run()
    {
        testDestroy();
        testCtors();
        testAssign();
        testGetStorage();
        testAccess();
        testIterators();
        testCapacity();
        testModifiers();
        testExceptions();
        testEquality();
        testHash();
    }
};

TEST_SUITE(array_test, "boost.json.array");

BOOST_JSON_NS_END
