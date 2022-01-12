//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

// Test that header file is self-contained.
#include <boost/json/object.hpp>

#include <boost/json/monotonic_resource.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>

#include <cmath>
#include <forward_list>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

#include "test.hpp"
#include "test_suite.hpp"
#include "checking_resource.hpp"

BOOST_JSON_NS_BEGIN

BOOST_STATIC_ASSERT( std::is_nothrow_destructible<object>::value );
BOOST_STATIC_ASSERT( std::is_nothrow_move_constructible<object>::value );

class object_test
{
public:
    static
    constexpr
    std::size_t
    size0_ =
        detail::small_object_size_ - 2;

    static
    constexpr
    std::size_t
    size1_ = size0_ + 4;

    test_suite::log_type log;
    string_view const str_;

    using init_list =
        std::initializer_list<
            std::pair<string_view, value_ref>>;

#define DECLARE_INIT_LISTS \
    init_list i0_ = { \
        {  "0",  0 }, {  "1",  1 }, {  "2",  2 }, {  "3",  3 }, {  "4",  4 }, \
        {  "5",  5 }, {  "6",  6 }, {  "7",  7 }, {  "8",  8 }, {  "9",  9 }, \
        { "10", 10 }, { "11", 11 }, { "12", 12 }, { "13", 13 }, { "14", 14 }, \
        { "15", 15 } }; \
    init_list i1_ = { \
        {  "0",  0 }, {  "1",  1 }, {  "2",  2 }, {  "3",  3 }, {  "4",  4 }, \
        {  "5",  5 }, {  "6",  6 }, {  "7",  7 }, {  "8",  8 }, {  "9",  9 }, \
        { "10", 10 }, { "11", 11 }, { "12", 12 }, { "13", 13 }, { "14", 14 }, \
        { "15", 15 }, { "16", 16 }, { "17", 17 }, { "18", 18 }, { "19", 19 } }

    string_view const s0_ =
        R"({"0":0,"1":1,"2":2,"3":3,"4":4,)"
        R"("5":5,"6":6,"7":7,"8":8,"9":9,)"
        R"("10":10,"11":11,"12":12,"13":13,"14":14,)"
        R"("15":15})";

    string_view const s1_ =
        R"({"0":0,"1":1,"2":2,"3":3,"4":4,)"
        R"("5":5,"6":6,"7":7,"8":8,"9":9,)"
        R"("10":10,"11":11,"12":12,"13":13,"14":14,)"
        R"("15":15,"16":16,"17":17,"18":18,"19":19})";

    object_test()
        : str_("abcdefghijklmnopqrstuvwxyz")
    {
        // ensure this string does
        // not fit in the SBO area.
        BOOST_ASSERT(str_.size() >
            string().capacity());

        DECLARE_INIT_LISTS;

        BOOST_TEST(
            i0_.size() == size0_);
        BOOST_TEST(
            i1_.size() == size1_);
    }

    template<class T, class U, class = void>
    struct is_equal_comparable : std::false_type {};

    template<class T, class U>
    struct is_equal_comparable<T, U, detail::void_t<decltype(
        std::declval<T const&>() == std::declval<U const&>()
            )>> : std::true_type {};

    template<class T, class U, class = void>
    struct is_unequal_comparable : std::false_type {};

    template<class T, class U>
    struct is_unequal_comparable<T, U, detail::void_t<decltype(
        std::declval<T const&>() != std::declval<U const&>()
            )>> : std::true_type {};

    BOOST_STATIC_ASSERT(  std::is_constructible<object::iterator, object::iterator>::value);
    BOOST_STATIC_ASSERT(! std::is_constructible<object::iterator, object::const_iterator>::value);

    BOOST_STATIC_ASSERT(  std::is_constructible<object::const_iterator, object::iterator>::value);
    BOOST_STATIC_ASSERT(  std::is_constructible<object::const_iterator, object::const_iterator>::value);

    BOOST_STATIC_ASSERT(  std::is_assignable<object::iterator&, object::iterator>::value);
    BOOST_STATIC_ASSERT(! std::is_assignable<object::iterator&, object::const_iterator>::value);

    BOOST_STATIC_ASSERT(  std::is_assignable<object::const_iterator&, object::iterator>::value);
    BOOST_STATIC_ASSERT(  std::is_assignable<object::const_iterator&, object::const_iterator>::value);

    BOOST_STATIC_ASSERT(is_equal_comparable<object::iterator, object::iterator>::value);
    BOOST_STATIC_ASSERT(is_equal_comparable<object::iterator, object::const_iterator>::value);

    BOOST_STATIC_ASSERT(is_equal_comparable<object::const_iterator, object::iterator>::value);
    BOOST_STATIC_ASSERT(is_equal_comparable<object::const_iterator, object::const_iterator>::value);

    BOOST_STATIC_ASSERT(is_unequal_comparable<object::iterator, object::iterator>::value);
    BOOST_STATIC_ASSERT(is_unequal_comparable<object::iterator, object::const_iterator>::value);

    BOOST_STATIC_ASSERT(is_unequal_comparable<object::const_iterator, object::iterator>::value);
    BOOST_STATIC_ASSERT(is_unequal_comparable<object::const_iterator, object::const_iterator>::value);

    BOOST_STATIC_ASSERT(  std::is_constructible<object::reverse_iterator, object::reverse_iterator>::value);
    // std::reverse_iterator ctor is not SFINAEd
    //BOOST_STATIC_ASSERT(! std::is_constructible<object::reverse_iterator, object::const_reverse_iterator>::value);

    BOOST_STATIC_ASSERT(  std::is_constructible<object::const_reverse_iterator, object::reverse_iterator>::value);
    BOOST_STATIC_ASSERT(  std::is_constructible<object::const_reverse_iterator, object::const_reverse_iterator>::value);

    BOOST_STATIC_ASSERT(  std::is_assignable<object::reverse_iterator&, object::reverse_iterator>::value);
    // std::reverse_iterator assignment is not SFINAEd
    //BOOST_STATIC_ASSERT(! std::is_assignable<object::reverse_iterator&, object::const_reverse_iterator>::value);

    BOOST_STATIC_ASSERT(  std::is_assignable<object::const_reverse_iterator&, object::reverse_iterator>::value);
    BOOST_STATIC_ASSERT(  std::is_assignable<object::const_reverse_iterator&, object::const_reverse_iterator>::value);

    BOOST_STATIC_ASSERT(is_equal_comparable<object::reverse_iterator, object::reverse_iterator>::value);
    BOOST_STATIC_ASSERT(is_equal_comparable<object::reverse_iterator, object::const_reverse_iterator>::value);

    BOOST_STATIC_ASSERT(is_equal_comparable<object::const_reverse_iterator, object::reverse_iterator>::value);
    BOOST_STATIC_ASSERT(is_equal_comparable<object::const_reverse_iterator, object::const_reverse_iterator>::value);

    BOOST_STATIC_ASSERT(is_unequal_comparable<object::reverse_iterator, object::reverse_iterator>::value);
    BOOST_STATIC_ASSERT(is_unequal_comparable<object::reverse_iterator, object::const_reverse_iterator>::value);

    BOOST_STATIC_ASSERT(is_unequal_comparable<object::const_reverse_iterator, object::reverse_iterator>::value);
    BOOST_STATIC_ASSERT(is_unequal_comparable<object::const_reverse_iterator, object::const_reverse_iterator>::value);

    static
    void
    check(
        object const& o,
        string_view s,
        std::size_t capacity = 0)
    {
        BOOST_TEST(
            parse(serialize(o)).as_object() ==
            parse(s).as_object());
        if(capacity != 0)
            BOOST_TEST(o.capacity() == capacity);
    }

    static
    void
    check(
        object const& o,
        std::size_t capacity)
    {
        BOOST_TEST(! o.empty());
        BOOST_TEST(o.size() == 3);
        BOOST_TEST(
            o.capacity() == capacity);
        BOOST_TEST(o.at("a").as_int64() == 1);
        BOOST_TEST(o.at("b").as_bool());
        BOOST_TEST(o.at("c").as_string() == "hello");
        check_storage(o, o.storage());
    }

    void
    testDtor()
    {
        // ~object()
        {
            object o;
        }
    }

    void
    testCtors()
    {
        DECLARE_INIT_LISTS;

        // object(detail::unchecked_object&&)
        {
            // small
            {
                value const jv = parse(s0_);
                check(jv.as_object(), s0_, i0_.size());
            }

            // small, duplicate
            {
                value const jv = parse(
                    R"({"0":0,"1":1,"2":2,"3":3,"4":4,)"
                    R"("5":5,"6":6,"7":7,"8":8,"9":9,)"
                    R"("10":10,"11":11,"12":12,"13":13,"14":14,)"
                    R"("15":15,"10":10})");
                check(jv.as_object(), s0_, i0_.size() + 1);
            }

            // large
            {
                value const jv = parse(s1_);
                check(jv.as_object(), s1_, i1_.size());
            }

            // large, duplicate
            {
                value const jv = parse(
                    R"({"0":0,"1":1,"2":2,"3":3,"4":4,)"
                    R"("5":5,"6":6,"7":7,"8":8,"9":9,)"
                    R"("10":10,"11":11,"12":12,"13":13,"14":14,)"
                    R"("15":15,"16":16,"17":17,"18":18,"19":19,"10":10})");
                check(jv.as_object(), s1_, i1_.size() + 1);
            }
        }

        // object()
        {
            object o;
            BOOST_TEST(o.empty());
            BOOST_TEST(o.size() == 0);
            BOOST_TEST(o.capacity() == 0);
        }

        // object(storage_ptr)
        fail_loop([&](storage_ptr const& sp)
        {
            object o(sp);
            check_storage(o, sp);
            BOOST_TEST(o.empty());
            BOOST_TEST(o.size() == 0);
            BOOST_TEST(o.capacity() == 0);
        });

        // object(std::size_t, storage_ptr)
        {
            // small
            {
                object o(size0_);
                BOOST_TEST(o.empty());
                BOOST_TEST(o.size() == 0);
                BOOST_TEST(o.capacity() == size0_);
            }

            // small
            fail_loop([&](storage_ptr const& sp)
            {
                object o(size0_, sp);
                check_storage(o, sp);
                BOOST_TEST(o.empty());
                BOOST_TEST(o.size() == 0);
                BOOST_TEST(o.capacity() == size0_);
            });

            // large
            {
                object o(size1_);
                BOOST_TEST(o.empty());
                BOOST_TEST(o.size() == 0);
                BOOST_TEST(o.capacity() == size1_);
            }

            // large
            fail_loop([&](storage_ptr const& sp)
            {
                object o(size1_, sp);
                check_storage(o, sp);
                BOOST_TEST(o.empty());
                BOOST_TEST(o.size() == 0);
                BOOST_TEST(o.capacity() == size1_);
            });
        }

        // object(InputIt, InputIt, size_type, storage_ptr)
        {
            // empty range
            {
                // random-access iterator
                std::vector<std::pair<string_view, value>> i1;
                object o1(i1.begin(), i1.end());
                BOOST_TEST(o1.empty());

                // bidirectional iterator
                std::map<string_view, value> i2;
                object o2(i2.begin(), i2.end());
                BOOST_TEST(o2.empty());

                // forward iterator
                std::forward_list<std::pair<string_view, value>> i3;
                object o3(i3.begin(), i3.end());
                BOOST_TEST(o3.empty());

                // input iterator
                auto const it = make_input_iterator(i3.begin());
                object o4(it, it);
                BOOST_TEST(o4.empty());
            }

            // small
            {
                object o(i0_.begin(), i0_.end());
                check(o, s0_, i0_.size());
            }

            // small, ForwardIterator
            fail_loop([&](storage_ptr const& sp)
            {
                object o(i0_.begin(), i0_.end(), size0_ + 1, sp);
                BOOST_TEST(! o.empty());
                check(o, s0_, size0_ + 1);
                check_storage(o, sp);
            });

            // small, InputIterator
            fail_loop([&](storage_ptr const& sp)
            {
                object o(
                    make_input_iterator(i0_.begin()),
                    make_input_iterator(i0_.end()), size0_ + 1, sp);
                BOOST_TEST(! o.empty());
                BOOST_TEST(o.capacity() == size0_ + 1);
                check(o, s0_, size0_ + 1);
                check_storage(o, sp);
            });

            // large
            {
                object o(i1_.begin(), i1_.end());
                check(o, s1_, i1_.size());
            }

            // large, ForwardIterator
            fail_loop([&](storage_ptr const& sp)
            {
                object o(i1_.begin(), i1_.end(), size1_ + 1, sp);
                BOOST_TEST(! o.empty());
                BOOST_TEST(o.capacity() == size1_ + 1);
                check(o, s1_, size1_ + 1);
                check_storage(o, sp);
            });

            // large, InputIterator
            fail_loop([&](storage_ptr const& sp)
            {
                object o(
                    make_input_iterator(i1_.begin()),
                    make_input_iterator(i1_.end()), size1_ + 1, sp);
                BOOST_TEST(! o.empty());
                BOOST_TEST(o.capacity() == size1_ + 1);
                check(o, s1_, size1_ + 1);
                check_storage(o, sp);
            });
        }

        // object(object&&)
        {
            object o1(i0_);
            check(o1, s0_);
            auto const sp =
                storage_ptr{};
            object o2(std::move(o1));
            BOOST_TEST(o1.empty());
            BOOST_TEST(o1.size() == 0);
            check(o2, s0_);
            check_storage(o1, sp);
            check_storage(o2, sp);
        }

        // object(object&&, storage_ptr)
        {
            // small
            fail_loop([&](storage_ptr const& sp)
            {
                object o1(i0_);
                object o2(std::move(o1), sp);
                BOOST_TEST(! o1.empty());
                check(o2, s0_, i0_.size());
                check_storage(o1,
                    storage_ptr{});
                check_storage(o2, sp);
            });

            // large
            fail_loop([&](storage_ptr const& sp)
            {
                object o1(i1_);
                object o2(std::move(o1), sp);
                BOOST_TEST(! o1.empty());
                check(o2, s1_, i1_.size());
                check_storage(o1,
                    storage_ptr{});
                check_storage(o2, sp);
            });
        }

        // object(pilfered<object>)
        {
            {
                auto const sp =
                    make_shared_resource<unique_resource>();
                object o1(i0_, sp);
                object o2(pilfer(o1));
                BOOST_TEST(
                    o1.storage() == storage_ptr());
                BOOST_TEST(
                    *o2.storage() == *sp);
                BOOST_TEST(o1.empty());
                check(o2, s0_, i0_.size());
            }

            // ensure pilfered-from objects
            // are trivially destructible
            {
                object o1(make_shared_resource<
                    monotonic_resource>());
                object o2(pilfer(o1));
                BOOST_TEST(o1.storage().get() ==
                    storage_ptr().get());
            }
        }

        auto const sp =
            make_shared_resource<
                unique_resource>();
        auto const sp0 = storage_ptr{};

        // object(object const&)
        {
            // small
            {
                object o1(i0_);
                object o2(o1);
                BOOST_TEST(! o1.empty());
                check(o2, s0_, i0_.size());
            }

            // large
            {
                object o1(i1_);
                object o2(o1);
                BOOST_TEST(! o1.empty());
                check(o2, s1_, i1_.size());
            }
        }

        // object(object const&, storage_ptr)
        {
            // small
            fail_loop([&](storage_ptr const& sp)
            {
                object o1(i0_);
                object o2(o1, sp);
                BOOST_TEST(! o1.empty());
                check(o2, s0_, i0_.size());
                check_storage(o2, sp);
            });

            // large
            fail_loop([&](storage_ptr const& sp)
            {
                object o1(i1_);
                object o2(o1, sp);
                BOOST_TEST(! o1.empty());
                check(o2, s1_, i1_.size());
                check_storage(o2, sp);
            });
        }

        // object(initializer_list, storage_ptr)
        {
            // small
            {
                object o(i0_);
                check(o, s0_, i0_.size());
            }

            // small
            fail_loop([&](storage_ptr const& sp)
            {
                object o(i0_, sp);
                check(o, s0_, i0_.size());
                check_storage(o, sp);
            });

            // large
            {
                object o(i1_);
                check(o, s1_, i1_.size());
            }

            // large
            fail_loop([&](storage_ptr const& sp)
            {
                object o(i1_, sp);
                check(o, s1_, i1_.size());
                check_storage(o, sp);
            });
        }

        // object(initializer_list, std::size_t, storage_ptr)
        {
            // small
            {
                object o(i0_, size0_ + 1);
                check(o, s0_, size0_ + 1);
            }

            // small
            fail_loop([&](storage_ptr const& sp)
            {
                object o(i0_, size0_ + 1, sp);
                BOOST_TEST(
                    *o.storage() == *sp);
                check(o, s0_, size0_ + 1);
            });
        }

        // operator=(object const&)
        {
            {
                object o1(i0_);
                object o2;
                o2 = o1;
                check(o1, s0_, i0_.size());
                check(o2, s0_, i0_.size());
                check_storage(o1,
                    storage_ptr{});
                check_storage(o2,
                    storage_ptr{});
            }

            fail_loop([&](storage_ptr const& sp)
            {
                object o1(i0_);
                object o2(sp);
                o2 = o1;
                check(o1, s0_, i0_.size());
                check(o2, s0_, i0_.size());
                check_storage(o1,
                    storage_ptr{});
                check_storage(o2, sp);
            });

            // self-assign
            {
                object o1(i0_);
                object const& o2(o1);
                o1 = o2;
                check(o1, s0_, i0_.size());
            }

            // copy from child
            {
                object o({
                    {"a", 1}, {"b",
                    { {"a", 1}, {"b", true}, {"c", "hello"} }
                    }, {"c", "hello"}});
                o = o["b"].as_object();
                check(o, 3);
            }
        }

        // operator=(object&&)
        {
            {
                object o1({
                    {"a", 1},
                    {"b", true},
                    {"c", "hello"}});
                object o2;
                o2 = std::move(o1);
                check(o2, 3);
                BOOST_TEST(o1.empty());
                check_storage(o1,
                    storage_ptr{});
                check_storage(o2,
                    storage_ptr{});
            }

            fail_loop([&](storage_ptr const& sp)
            {
                object o1({
                    {"a", 1},
                    {"b", true},
                    {"c", "hello"}});
                object o2(sp);
                o2 = std::move(o1);
                check(o1, 3);
                check(o2, 3);
                check_storage(o1,
                    storage_ptr{});
                check_storage(o2, sp);
            });

            // self-move
            {
                object o1({
                    {"a", 1},
                    {"b", true},
                    {"c", "hello"}});
                object const& o2(o1);
                o1 = std::move(o2);
                check(o1, 3);
            }

            // move from child
            {
                object o({
                    {"a", 1}, {"b",
                    { {"a", 1}, {"b", true}, {"c", "hello"} }
                    }, {"c", "hello"}});
                o = std::move(o["b"].as_object());
                check(o, 3);
            }
        }

        // operator=(initializer_list)
        {
            {
                object o;
                o = {
                    {"a", 1},
                    {"b", true},
                    {"c", "hello"} },
                check(o, 3);
                check_storage(o,
                    storage_ptr{});
            }

            fail_loop([&](storage_ptr const& sp)
            {
                object o(sp);
                o = {
                    {"a", 1},
                    {"b", true},
                    {"c", "hello"} },
                BOOST_TEST(
                    *o.storage() == *sp);
                check(o, 3);
                check_storage(o, sp);
            });

            // assign from child
            {
                object o = {
                    { "k1", 1 },
                    { "k2", 2 },
                    { "k3", 3 } };
                o = { { "k2", o["k2"] } };
                BOOST_TEST(
                    o == object({ { "k2", 2 } }));
            }
        }
    }

    void
    testIterators()
    {
        object o({
            {"a", 1},
            {"b", true},
            {"c", "hello"}});
        auto const& co = o;
        object no;
        auto const& cno = no;

        // empty container
        {
            BOOST_TEST(no.begin() == no.end());
            BOOST_TEST(cno.begin() == cno.end());
            BOOST_TEST(no.cbegin() == no.cend());
        }

        // begin()
        {
            auto it = o.begin();
            BOOST_TEST(it->key() == "a"); ++it;
            BOOST_TEST(it->key() == "b"); it++;
            BOOST_TEST(it->key() == "c"); ++it;
            BOOST_TEST(it == o.end());
        }

        // begin() const
        {
            auto it = co.begin();
            BOOST_TEST(it->key() == "a"); ++it;
            BOOST_TEST(it->key() == "b"); it++;
            BOOST_TEST(it->key() == "c"); ++it;
            BOOST_TEST(it == co.end());
        }

        // cbegin()
        {
            auto it = o.cbegin();
            BOOST_TEST(it->key() == "a"); ++it;
            BOOST_TEST(it->key() == "b"); it++;
            BOOST_TEST(it->key() == "c"); ++it;
            BOOST_TEST(it == o.cend());
        }

        // end()
        {
            auto it = o.end();
            --it; BOOST_TEST(it->key() == "c");
            it--; BOOST_TEST(it->key() == "b");
            --it; BOOST_TEST(it->key() == "a");
            BOOST_TEST(it == o.begin());
        }

        // end() const
        {
            auto it = co.end();
            --it; BOOST_TEST(it->key() == "c");
            it--; BOOST_TEST(it->key() == "b");
            --it; BOOST_TEST(it->key() == "a");
            BOOST_TEST(it == co.begin());
        }

        // cend()
        {
            auto it = o.cend();
            --it; BOOST_TEST(it->key() == "c");
            it--; BOOST_TEST(it->key() == "b");
            --it; BOOST_TEST(it->key() == "a");
            BOOST_TEST(it == o.cbegin());
        }

        // rbegin()
        {
            auto it = o.rbegin();
            BOOST_TEST(it->key() == "c"); ++it;
            BOOST_TEST(it->key() == "b"); it++;
            BOOST_TEST(it->key() == "a"); ++it;
            BOOST_TEST(it == o.rend());
        }

        // rbegin() const
        {
            auto it = co.rbegin();
            BOOST_TEST(it->key() == "c"); ++it;
            BOOST_TEST(it->key() == "b"); it++;
            BOOST_TEST(it->key() == "a"); ++it;
            BOOST_TEST(it == co.rend());
        }

        // crbegin()
        {
            auto it = o.crbegin();
            BOOST_TEST(it->key() == "c"); ++it;
            BOOST_TEST(it->key() == "b"); it++;
            BOOST_TEST(it->key() == "a"); ++it;
            BOOST_TEST(it == o.crend());
        }

        // rend()
        {
            auto it = o.rend();
            --it; BOOST_TEST(it->key() == "a");
            it--; BOOST_TEST(it->key() == "b");
            --it; BOOST_TEST(it->key() == "c");
            BOOST_TEST(it == o.rbegin());
        }

        // rend() const
        {
            auto it = co.rend();
            --it; BOOST_TEST(it->key() == "a");
            it--; BOOST_TEST(it->key() == "b");
            --it; BOOST_TEST(it->key() == "c");
            BOOST_TEST(it == co.rbegin());
        }

        // crend()
        {
            auto it = o.crend();
            --it; BOOST_TEST(it->key() == "a");
            it--; BOOST_TEST(it->key() == "b");
            --it; BOOST_TEST(it->key() == "c");
            BOOST_TEST(it == o.crbegin());
        }
    }

    //------------------------------------------------------

    void
    testCapacity()
    {
        BOOST_TEST(
            object{}.size() < object{}.max_size());
    }

    //------------------------------------------------------

    void
    testModifiers()
    {
        DECLARE_INIT_LISTS;

        // clear
        {
            // empty
            {
                object o;
                o.clear();
                BOOST_TEST(o.empty());
            }

            // small
            {
                object o(i0_);
                BOOST_TEST(! o.empty());
                o.clear();
                BOOST_TEST(o.empty());
            }

            // large
            {
                object o(i1_);
                BOOST_TEST(! o.empty());
                o.clear();
                BOOST_TEST(o.empty());
            }
        }

        // insert(P&&)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                object o(sp);
                auto result = o.insert(
                    std::make_pair("x", 1));
                BOOST_TEST(result.second);
                BOOST_TEST(result.first->key() == "x");
                BOOST_TEST(result.first->value().as_int64() == 1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                object o(sp);
                auto const p = std::make_pair("x", 1);
                auto result = o.insert(p);
                BOOST_TEST(result.second);
                BOOST_TEST(result.first->key() == "x");
                BOOST_TEST(result.first->value().as_int64() == 1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                object o({
                    {"a", 1},
                    {"b", 2},
                    {"c", 3}}, sp);
                auto const result = o.insert(
                    std::make_pair("b", 4));
                BOOST_TEST(
                    result.first->value().as_int64() == 2);
                BOOST_TEST(! result.second);
            });

            // insert child
            {
                object o = {
                    { "k1", 1 },
                    { "k2", 2 },
                    { "k3", 3 } };
                o.insert(std::pair<
                    string_view, value&>(
                        "k4", o["k2"]));
                BOOST_TEST(o == object({
                    { "k1", 1 },
                    { "k2", 2 },
                    { "k3", 3 },
                    { "k4", 2 }}));
            }
        }

        // insert(InputIt, InputIt)
        {
            // small
            {
                // ForwardIterator
                fail_loop([&](storage_ptr const& sp)
                {
                    object o(sp);
                    o.insert(i0_.begin(), i0_.end());
                    check(o, s0_);
                });

                // InputIterator
                fail_loop([&](storage_ptr const& sp)
                {
                    object o(sp);
                    o.insert(
                        make_input_iterator(i0_.begin()),
                        make_input_iterator(i0_.end()));
                    check(o, s0_);
                });

                // existing duplicate key, ForwardIterator
                {
                    object o({{"0",0},{"1",1},{"2",2}});
                    init_list i = {{"2",nullptr},{"3",3}};
                    o.insert(i.begin(), i.end());
                    BOOST_TEST(o.capacity() <=
                        detail::small_object_size_);
                    check(o, R"({"0":0,"1":1,"2":2,"3":3})");
                }

                // existing duplicate key, InputIterator
                {
                    object o({{"0",0},{"1",1},{"2",2}});
                    init_list i = {{"2",nullptr},{"3",3}};
                    o.insert(
                        make_input_iterator(i.begin()),
                        make_input_iterator(i.end()));
                    BOOST_TEST(o.capacity() <=
                        detail::small_object_size_);
                    check(o, R"({"0":0,"1":1,"2":2,"3":3})");
                }

                // new duplicate key, ForwardIterator
                {
                    object o({{"0",0},{"1",1},{"2",2}});
                    init_list i = {{"3",3},{"4",4},{"3",5}};
                    o.insert(i.begin(), i.end());
                    BOOST_TEST(o.capacity() <=
                        detail::small_object_size_);
                    check(o, R"({"0":0,"1":1,"2":2,"3":3,"4":4})");
                }

                // new duplicate key, InputIterator
                {
                    object o({{"0",0},{"1",1},{"2",2}});
                    init_list i = {{"3",3},{"4",4},{"3",5}};
                    o.insert(
                        make_input_iterator(i.begin()),
                        make_input_iterator(i.end()));
                    BOOST_TEST(o.capacity() <=
                        detail::small_object_size_);
                    check(o, R"({"0":0,"1":1,"2":2,"3":3,"4":4})");
                }
            }

            // large, ForwardIterator
            fail_loop([&](storage_ptr const& sp)
            {
                object o(sp);
                o.insert(i1_.begin(), i1_.end());
                check(o, s1_);
            });

            // large, InputIterator
            fail_loop([&](storage_ptr const& sp)
            {
                object o(sp);
                o.insert(
                    make_input_iterator(i1_.begin()),
                    make_input_iterator(i1_.end()));
                check(o, s1_);
            });
        }

        // insert(initializer_list)
        {
            // small
            fail_loop([&](storage_ptr const& sp)
            {
                object o(sp);
                o.insert(i0_);
                check(o, s0_);
            });

            // small, existing duplicate
            fail_loop([&](storage_ptr const& sp)
            {
                object o({{"0",0},{"1",1},/*{"2",2},*/{"3",3},{"4",4}}, sp);
                BOOST_TEST(o.capacity() <= detail::small_object_size_);
                o.insert({{"2",2},{"3",3}});
                BOOST_TEST(o.capacity() <= detail::small_object_size_);
                check(o, R"({"0":0,"1":1,"2":2,"3":3,"4":4})");
            });

            // small, new duplicate
            fail_loop([&](storage_ptr const& sp)
            {
                object o({{"0",0},{"1",1},/*{"2",2},{"3",3},*/{"4",4}}, sp);
                BOOST_TEST(o.capacity() <= detail::small_object_size_);
                o.insert({{"2",2},{"3",3},{"2",2}});
                BOOST_TEST(o.capacity() <= detail::small_object_size_);
                check(o, R"({"0":0,"1":1,"2":2,"3":3,"4":4})");
            });

            // large
            fail_loop([&](storage_ptr const& sp)
            {
                object o(sp);
                o.insert(i1_);
                check(o, s1_);
            });

            // large, existing duplicate
            fail_loop([&](storage_ptr const& sp)
            {
                object o({
                    {"0",0},{"1",1},{"2",2},{"3",3},{"4",4},
                    {"5",5},{"6",6},{"7",7},{"8",8},{"9",9},
                    /*{"10",10},*/{"11",11},{"12",12},{"13",13},{"14",14},
                    {"15",15},{"16",16},{"17",17},{"18",18},{"19",19}}, sp);
                BOOST_TEST(o.capacity() > detail::small_object_size_);
                o.insert({{"10",10},{"11",11}});
                BOOST_TEST(o.capacity() > detail::small_object_size_);
                check(o, s1_);
            });

            // large, new duplicate
            fail_loop([&](storage_ptr const& sp)
            {
                object o({
                    {"0",0},{"1",1},{"2",2},{"3",3},{"4",4},
                    {"5",5},{"6",6},{"7",7},{"8",8},{"9",9},
                    /*{"10",10},{"11",11},*/{"12",12},{"13",13},{"14",14},
                    {"15",15},{"16",16},{"17",17},{"18",18},{"19",19}},
                    detail::small_object_size_ + 1, sp);
                BOOST_TEST(o.capacity() > detail::small_object_size_);
                o.insert({{"10",10},{"11",11},{"10",10}});
                BOOST_TEST(o.capacity() > detail::small_object_size_);
                check(o, s1_);
            });

            // do rollback in ~revert_insert
            fail_loop([&](storage_ptr const& sp)
            {
                object o(sp);
                o.insert({
                    { "a", { 1, 2, 3, 4 } } });
            });

            // insert child
            {
                object o = {
                    { "k1", 1 },
                    { "k2", 2 },
                    { "k3", 3 } };
                o.insert({
                    { "k4", o["k2"] } });
                BOOST_TEST(o == object({
                    { "k1", 1 },
                    { "k2", 2 },
                    { "k3", 3 },
                    { "k4", 2 }}));
            }
        }

        // insert_or_assign(key, o);
        {
            fail_loop([&](storage_ptr const& sp)
            {
                object o({{"a", 1}}, sp);
                o.insert_or_assign("a", str_);
                BOOST_TEST(o["a"].is_string());
            });

            fail_loop([&](storage_ptr const& sp)
            {
                object o({
                    {"a", 1},
                    {"b", 2},
                    {"c", 3}}, sp);
                o.insert_or_assign("d", str_);
                BOOST_TEST(o["d"].is_string());
                BOOST_TEST(o.size() == 4);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                object o({{"a", 1}}, sp);
                o.insert_or_assign("b", true);
                o.insert_or_assign("c", "hello");
                check(o, 3);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                object o({{"a", 1}}, sp);
                BOOST_TEST(
                    ! o.insert_or_assign("a", 2).second);
                BOOST_TEST(o["a"].as_int64() == 2);
            });

            // insert child
            {
                object o = {
                    { "k1", 1 },
                    { "k2", 2 },
                    { "k3", 3 } };
                o.insert_or_assign(
                    "k4", o["k2"]);
                BOOST_TEST(o == object({
                    { "k1", 1 },
                    { "k2", 2 },
                    { "k3", 3 },
                    { "k4", 2 }}));
            }
        }

        // emplace(key, arg)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                object o(sp);
                o.emplace("a", 1);
                o.emplace("b", true);
                o.emplace("c", "hello");
                check(o, 3);
            });

            // emplace child
            {
                object o = {
                    { "k1", 1 },
                    { "k2", 2 },
                    { "k3", 3 } };
                o.emplace(
                    "k4", o["k2"]);
                BOOST_TEST(o == object({
                    { "k1", 1 },
                    { "k2", 2 },
                    { "k3", 3 },
                    { "k4", 2 }}));
            }
        }

        // erase(pos)
        {
            // small
            {
                object o(i0_);
                auto it = o.erase(o.find("10"));
                BOOST_TEST(it->key() == "15");
                BOOST_TEST(
                    it->value().as_int64() == 15);
                BOOST_TEST(serialize(o) ==
                    R"({"0":0,"1":1,"2":2,"3":3,"4":4,)"
                    R"("5":5,"6":6,"7":7,"8":8,"9":9,)"
                    R"("15":15,"11":11,"12":12,"13":13,"14":14})");
            }

            // large
            {
                object o(i1_);
                auto it = o.erase(o.find("10"));
                BOOST_TEST(it->key() == "19");
                BOOST_TEST(
                    it->value().as_int64() == 19);
                BOOST_TEST(serialize(o) ==
                    R"({"0":0,"1":1,"2":2,"3":3,"4":4,)"
                    R"("5":5,"6":6,"7":7,"8":8,"9":9,)"
                    R"("19":19,"11":11,"12":12,"13":13,"14":14,)"
                    R"("15":15,"16":16,"17":17,"18":18})");
            }
        }

        // erase(key)
        {
            {
                object o({
                    {"a", 1},
                    {"b", true},
                    {"c", "hello"}});
                BOOST_TEST(o.erase("b2") == 0);
                check(o, 3);
            }

            {
                object o({
                    {"a", 1},
                    {"b", true},
                    {"b2", 2},
                    {"c", "hello"}});
                BOOST_TEST(o.erase("b2") == 1);
                check(o, 4);
            }
        }

        // swap(object&)
        {
            {
                object o1 = {{"a",1}, {"b",true}, {"c", "hello"}};
                object o2 = {{"d",{1,2,3}}};
                swap(o1, o2);
                BOOST_TEST(o1.size() == 1);
                BOOST_TEST(o2.size() == 3);
                BOOST_TEST(o1.count("d") == 1);
            }

            fail_loop([&](storage_ptr const& sp)
            {
                object o1 = {{"a",1}, {"b",true}, {"c", "hello"}};
                object o2({{"d",{1,2,3}}}, sp);
                swap(o1, o2);
                BOOST_TEST(o1.size() == 1);
                BOOST_TEST(o2.size() == 3);
                BOOST_TEST(o1.count("d") == 1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                object o1({{"d",{1,2,3}}}, sp);
                object o2 = {{"a",1}, {"b",true}, {"c", "hello"}};
                swap(o1, o2);
                BOOST_TEST(o1.size() == 3);
                BOOST_TEST(o2.size() == 1);
                BOOST_TEST(o2.count("d") == 1);
            });
        }
    }

    //------------------------------------------------------

    void
    testLookup()
    {
        object o0;
        object o1({
            {"a", 1},
            {"b", true},
            {"c", "hello"}});
        auto const& co0 = o0;
        auto const& co1 = o1;

        // at(key)
        {
            BOOST_TEST(
                o1.at("a").is_number());
            BOOST_TEST_THROWS((o1.at("d")),
                std::out_of_range);
        }

        // at(key) const
        {
            BOOST_TEST(
                co1.at("a").is_number());
            BOOST_TEST_THROWS((co1.at("d")),
                std::out_of_range);
        }

        // operator[&](key)
        {
            object o({
                {"a", 1},
                {"b", true},
                {"c", "hello"}});
            BOOST_TEST(o.count("d") == 0);;
            BOOST_TEST(o["a"].is_number());
            BOOST_TEST(o["d"].is_null());
            BOOST_TEST(o.count("d") == 1);
        }

        // count(key)
        {
            BOOST_TEST(o1.count("a") == 1);
            BOOST_TEST(o1.count("d") == 0);
            BOOST_TEST(o1.count("e") == 0);
        }

        // find(key)
        // find(key) const
        {
            BOOST_TEST(
                o0.find("") == o0.end());
            BOOST_TEST(
                o1.find("a")->key() == "a");
            BOOST_TEST(
                o1.find("e") == o1.end());

            BOOST_TEST(
                co0.find("") == co0.end());
            BOOST_TEST(
                co1.find("a")->key() == "a");
            BOOST_TEST(
                co1.find("e") == o1.end());
        }

        // contains(key)
        {
            BOOST_TEST(o1.contains("a"));
            BOOST_TEST(! o1.contains("e"));
            BOOST_TEST(! object().contains(""));
        }

        // if_contains(key)
        // if_contains(key) const
        {
            BOOST_TEST(o1.if_contains("a")->is_int64());
            BOOST_TEST(o1.if_contains("e") == nullptr);
            BOOST_TEST(co1.if_contains("a")->is_int64());
            BOOST_TEST(co1.if_contains("e") == nullptr);

            *o1.if_contains("a") = 2;
            BOOST_TEST(co1.if_contains("a")->as_int64() == 2);
        }
    }

    void
    testHashPolicy()
    {
        // reserve(size_type)
        {
            {
                object o;
                for(std::size_t i = 0; i < 10; ++i)
                    o.emplace(std::to_string(i), i);
                o.reserve(15);
                BOOST_TEST(o.capacity() >= 15);
                o.reserve(20);
                BOOST_TEST(o.capacity() >= 20);
            }

            {
                object o;
                o.reserve(3);
                BOOST_TEST(o.capacity() == 3);
                o.reserve(7);
                BOOST_TEST(o.capacity() == 7);
            }
        }
    }

    //------------------------------------------------------

    void
    testImplementation()
    {
        // insert duplicate keys
        {
            object o({
                {"a", 1},
                {"b", true},
                {"b", {1,2,3}},
                {"c", "hello"}});
            BOOST_TEST(o.at("a").as_int64() == 1);
            BOOST_TEST(o.at("b").as_bool() == true);
            BOOST_TEST(o.at("c").as_string() == "hello");
        }

        // destroy key_value_pair array with need_free=false
        {
            monotonic_resource mr;
            object o({
                {"a", 1},
                {"b", true},
                {"b", {1,2,3}},
                {"c", "hello"}}, &mr);
        }
    }

    static
    string_view
    make_key(
        std::size_t i,
        char* buf)
    {
        int constexpr base = 62;
        char const* const alphabet =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        char* dest = buf;
        do
        {
            *dest++ = alphabet[i%base];
            i /= base;
        }
        while(i);
        return { buf, static_cast<
            std::size_t>(dest-buf) };
    }

    void
    testCollisions()
    {
        int constexpr buckets =
            detail::small_object_size_ + 1;
        int constexpr collisions = 3;

        //DECLARE_INIT_LISTS;

        // find a set of keys that collide
        std::vector<std::string> v;
        object o;
        o.reserve(buckets);
        {
            BOOST_TEST(
                o.capacity() == buckets);
            char buf[26];
            auto const match =
                o.t_->digest("0") % buckets;
            v.push_back("0");
            std::size_t i = 1;
            for(;;)
            {
                auto s = make_key(i, buf);
                if((o.t_->digest(s) %
                    buckets) == match)
                {
                    v.push_back(std::string(
                        s.data(), s.size()));
                    if(v.size() >= collisions)
                        break;
                }
                ++i;
            }
        }

        // ensure collisions are distinguishable
        {
            o.clear();
            BOOST_TEST(
                (o.t_->digest(v[0]) % buckets) ==
                (o.t_->digest(v[1]) % buckets));
            BOOST_TEST(
                (o.t_->digest(v[1]) % buckets) ==
                (o.t_->digest(v[2]) % buckets));
            o.emplace(v[0], 1);
            o.emplace(v[1], 2);
            o.emplace(v[2], 3);
            BOOST_TEST(o.at(v[0]).to_number<int>() == 1);
            BOOST_TEST(o.at(v[1]).to_number<int>() == 2);
            BOOST_TEST(o.at(v[2]).to_number<int>() == 3);
        }

        // erase k1
        {
            o.clear();
            o.emplace(v[0], 1);
            o.emplace(v[1], 2);
            o.emplace(v[2], 3);
            o.erase(v[0]);
            BOOST_TEST(o.at(v[1]).to_number<int>() == 2);
            BOOST_TEST(o.at(v[2]).to_number<int>() == 3);
        }

        // erase k2
        {
            o.clear();
            o.emplace(v[0], 1);
            o.emplace(v[1], 2);
            o.emplace(v[2], 3);
            o.erase(v[1]);
            BOOST_TEST(o.at(v[0]).to_number<int>() == 1);
            BOOST_TEST(o.at(v[2]).to_number<int>() == 3);
        }

        // erase k3
        {
            o.clear();
            o.emplace(v[0], 1);
            o.emplace(v[1], 2);
            o.emplace(v[2], 3);
            o.erase(v[2]);
            BOOST_TEST(o.at(v[0]).to_number<int>() == 1);
            BOOST_TEST(o.at(v[1]).to_number<int>() == 2);
        }
    }

    void
    testEquality()
    {
        BOOST_TEST(object({}) == object({}));
        BOOST_TEST(object({}) != object({{"1",1},{"2",2}}));
        BOOST_TEST(object({{"1",1},{"2",2},{"3",3}}) == object({{"1",1},{"2",2},{"3",3}}));
        BOOST_TEST(object({{"1",1},{"2",2},{"3",3}}) != object({{"1",1},{"2",2}}));
        BOOST_TEST(object({{"1",1},{"2",2},{"3",3}}) == object({{"3",3},{"2",2},{"1",1}}));
    }

    void
    testAllocation()
    {
        {
            checking_resource res;
            object o(&res);
            o.reserve(1);
        }

        {
            checking_resource res;
            object o(&res);
            o.reserve(1000);
        }

        {
            checking_resource res;
            object o({std::make_pair("one", 1)}, &res);
        }
    }

    void
    testHash()
    {
        BOOST_TEST(check_hash_equal(
            object(), object({})));
        BOOST_TEST(expect_hash_not_equal(
            object(), object({{"1",1},{"2",2}})));
        BOOST_TEST(check_hash_equal(
            object({{"a",1}, {"b",2}, {"c",3}}),
            object({{"b",2}, {"c",3}, {"a",1}})));
        BOOST_TEST(expect_hash_not_equal(
            object({{"a",1}, {"b",2}, {"c",3}}),
            object({{"b",2}, {"c",3}})));
    }

    void
    run()
    {
        testDtor();
        testCtors();
        testIterators();
        testCapacity();
        testModifiers();
        testLookup();
        testHashPolicy();
        testImplementation();
        testCollisions();
        testEquality();
        testAllocation();
        testHash();
    }
};

TEST_SUITE(object_test, "boost.json.object");

BOOST_JSON_NS_END
