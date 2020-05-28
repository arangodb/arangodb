/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    pack.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/pack.hpp>
#include <boost/hof/always.hpp>
#include <boost/hof/identity.hpp>
#include <memory>
#include "test.hpp"

BOOST_HOF_TEST_CASE()
{
    auto p1 = boost::hof::pack_basic(1, 2);
    auto p2 = p1;
    BOOST_HOF_TEST_CHECK(p2(binary_class()) == p1(binary_class()));
    
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_basic(1, 2)(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_basic(1, 2)(binary_class()) == 3 );

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack(1, 2)(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack(1, 2)(binary_class()) == 3 );

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_forward(1, 2)(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_forward(1, 2)(binary_class()) == 3 );
}
#if BOOST_HOF_HAS_NOEXCEPT_DEDUCTION
struct copy_throws 
{
    copy_throws() {}
    copy_throws(copy_throws const&) {}
    copy_throws(copy_throws&&) noexcept {}
};

BOOST_HOF_TEST_CASE()
{
    int i = 1;
    copy_throws ct{};
    static_assert(!noexcept(boost::hof::pack(ct, ct)(boost::hof::always())), "noexcept pack");
    static_assert(noexcept(boost::hof::pack(1, 2)(boost::hof::always())), "noexcept pack");
    static_assert(noexcept(boost::hof::pack_forward(ct, ct)(boost::hof::always())), "noexcept pack");
    static_assert(noexcept(boost::hof::pack_forward(i, i)(boost::hof::always())), "noexcept pack");
    static_assert(noexcept(boost::hof::pack_forward(1, 2)(boost::hof::always())), "noexcept pack");
    static_assert(noexcept(boost::hof::pack_basic(ct, ct)(boost::hof::always())), "noexcept pack");
    static_assert(noexcept(boost::hof::pack_basic(i, i)(boost::hof::always())), "noexcept pack");
    static_assert(noexcept(boost::hof::pack_basic(1, 2)(boost::hof::always())), "noexcept pack");

    static_assert(noexcept(boost::hof::pack()(boost::hof::always())), "noexcept pack");
    static_assert(noexcept(boost::hof::pack_forward()(boost::hof::always())), "noexcept pack");
    static_assert(noexcept(boost::hof::pack_basic()(boost::hof::always())), "noexcept pack");
}

BOOST_HOF_TEST_CASE()
{
    copy_throws ct{};
    static_assert(!noexcept(boost::hof::pack_join(boost::hof::pack(ct), boost::hof::pack(ct))(boost::hof::always())), "noexcept pack");
    static_assert(noexcept(boost::hof::pack_join(boost::hof::pack(1), boost::hof::pack(1))(boost::hof::always())), "noexcept pack");
    static_assert(noexcept(boost::hof::pack_join(boost::hof::pack(), boost::hof::pack())(boost::hof::always())), "noexcept pack");
    auto p = boost::hof::pack(1);
    static_assert(noexcept(boost::hof::pack_join(p, boost::hof::pack())(boost::hof::always())), "noexcept pack");
    static_assert(noexcept(boost::hof::pack_join(boost::hof::pack(), p)(boost::hof::always())), "noexcept pack");
    static_assert(noexcept(boost::hof::pack_join(p, p)(boost::hof::always())), "noexcept pack");
    auto pt = boost::hof::pack(ct);
    static_assert(!noexcept(boost::hof::pack_join(pt, boost::hof::pack())(boost::hof::always())), "noexcept pack");
    static_assert(!noexcept(boost::hof::pack_join(boost::hof::pack(), pt)(boost::hof::always())), "noexcept pack");
    static_assert(!noexcept(boost::hof::pack_join(pt, pt)(boost::hof::always())), "noexcept pack");

}
#endif
BOOST_HOF_TEST_CASE()
{
    static constexpr int x = 1;
    static constexpr int y = 2;

    auto p1 = boost::hof::pack_basic(x, y);
    static_assert(!boost::hof::detail::is_default_constructible<decltype(p1)>::value, "Pack default constructible");
    
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_basic(x, y)(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_basic(x, y)(binary_class()) == 3 );

    auto p2 = boost::hof::pack(std::ref(x), std::ref(y));
    static_assert(!boost::hof::detail::is_default_constructible<decltype(p2)>::value, "Pack default constructible");

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack(x, y)(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack(std::ref(x), std::ref(y))(binary_class()) == 3 );

    auto p3 = boost::hof::pack_forward(x, y);
    static_assert(!boost::hof::detail::is_default_constructible<decltype(p3)>::value, "Pack default constructible");

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_forward(x, y)(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_forward(x, y)(binary_class()) == 3 );
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_basic()(boost::hof::always(3)) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_basic()(boost::hof::always(3)) == 3 );
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_basic(3)(boost::hof::identity) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_basic(3)(boost::hof::identity) == 3 );
}

BOOST_HOF_TEST_CASE()
{
    auto p = boost::hof::pack(1);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_join(p, boost::hof::pack(2))(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_join(p, p)(binary_class()) == 2);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_join(p, p, boost::hof::pack())(binary_class()) == 2);
    
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_basic(1), boost::hof::pack_basic(2))(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_basic(1), boost::hof::pack_basic(2))(binary_class()) == 3 );

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_join(boost::hof::pack(1), boost::hof::pack(2))(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_join(boost::hof::pack(1), boost::hof::pack(2))(binary_class()) == 3 );

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_forward(1), boost::hof::pack_forward(2))(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_forward(1), boost::hof::pack_forward(2))(binary_class()) == 3 );
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_basic(), boost::hof::pack_basic(1, 2))(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_basic(), boost::hof::pack_basic(1, 2))(binary_class()) == 3 );

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_join(boost::hof::pack(), boost::hof::pack(1, 2))(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_join(boost::hof::pack(), boost::hof::pack(1, 2))(binary_class()) == 3 );

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_forward(), boost::hof::pack_forward(1, 2))(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_forward(), boost::hof::pack_forward(1, 2))(binary_class()) == 3 );
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_basic(1, 2), boost::hof::pack_basic())(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_basic(1, 2), boost::hof::pack_basic())(binary_class()) == 3 );

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_join(boost::hof::pack(1, 2), boost::hof::pack())(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_join(boost::hof::pack(1, 2), boost::hof::pack())(binary_class()) == 3 );

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_forward(1, 2), boost::hof::pack_forward())(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_forward(1, 2), boost::hof::pack_forward())(binary_class()) == 3 );
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_basic(1), boost::hof::pack_basic(), boost::hof::pack_basic(2))(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_basic(1), boost::hof::pack_basic(), boost::hof::pack_basic(2))(binary_class()) == 3 );

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_join(boost::hof::pack(1), boost::hof::pack(), boost::hof::pack(2))(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_join(boost::hof::pack(1), boost::hof::pack(), boost::hof::pack(2))(binary_class()) == 3 );

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_forward(1), boost::hof::pack_forward(), boost::hof::pack_forward(2))(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_forward(1), boost::hof::pack_forward(), boost::hof::pack_forward(2))(binary_class()) == 3 );
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_basic(), boost::hof::pack_basic(1), boost::hof::pack_basic(), boost::hof::pack_basic(2))(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_basic(), boost::hof::pack_basic(1), boost::hof::pack_basic(), boost::hof::pack_basic(2))(binary_class()) == 3 );

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_join(boost::hof::pack(), boost::hof::pack(1), boost::hof::pack(), boost::hof::pack(2))(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_join(boost::hof::pack(), boost::hof::pack(1), boost::hof::pack(), boost::hof::pack(2))(binary_class()) == 3 );

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_forward(), boost::hof::pack_forward(1), boost::hof::pack_forward(), boost::hof::pack_forward(2))(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_forward(), boost::hof::pack_forward(1), boost::hof::pack_forward(), boost::hof::pack_forward(2))(binary_class()) == 3 );
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_basic(1), boost::hof::pack_basic(), boost::hof::pack_basic(2), boost::hof::pack_basic())(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_basic(1), boost::hof::pack_basic(), boost::hof::pack_basic(2), boost::hof::pack_basic())(binary_class()) == 3 );

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_join(boost::hof::pack(1), boost::hof::pack(), boost::hof::pack(2), boost::hof::pack())(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_join(boost::hof::pack(1), boost::hof::pack(), boost::hof::pack(2), boost::hof::pack())(binary_class()) == 3 );

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_forward(1), boost::hof::pack_forward(), boost::hof::pack_forward(2), boost::hof::pack_forward())(binary_class()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_forward(1), boost::hof::pack_forward(), boost::hof::pack_forward(2), boost::hof::pack_forward())(binary_class()) == 3 );
}

struct deref
{
    int operator()(const std::unique_ptr<int>& i) const
    {
        return *i;
    }
};

BOOST_HOF_TEST_CASE()
{
    std::unique_ptr<int> i(new int(3));
    BOOST_HOF_TEST_CHECK(boost::hof::pack_basic(i)(deref()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_basic(std::unique_ptr<int>(new int(3)))(deref()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_forward(std::unique_ptr<int>(new int(3)))(deref()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack(std::unique_ptr<int>(new int(3)))(deref()) == 3);
    auto p = boost::hof::pack_basic(std::unique_ptr<int>(new int(3)));
    BOOST_HOF_TEST_CHECK(p(deref()) == 3);

    BOOST_HOF_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_basic(), boost::hof::pack_basic(std::unique_ptr<int>(new int(3))))(deref()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_join(boost::hof::pack_forward(), boost::hof::pack_forward(std::unique_ptr<int>(new int(3))))(deref()) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::pack_join(boost::hof::pack(), boost::hof::pack(std::unique_ptr<int>(new int(3))))(deref()) == 3);
    // BOOST_HOF_TEST_CHECK(p(deref()) == 3);
}

struct move_rvalue 
{
    void operator()(std::string&& s) const 
    {
        std::string ss = std::move(s);
        BOOST_HOF_TEST_CHECK(ss == "abcdef");
        s = "00000";
    }
};

struct check_rvalue 
{
    void operator()(std::string&& s) const 
    {
        BOOST_HOF_TEST_CHECK(s == "abcdef");
    }
};

BOOST_HOF_TEST_CASE()
{
    auto p = boost::hof::pack_basic(std::string{"abcdef"});
    p(move_rvalue{});
    p(check_rvalue{});
}

BOOST_HOF_TEST_CASE()
{
    auto p = boost::hof::pack(std::string{"abcdef"});
    p(move_rvalue{});
    p(check_rvalue{});
}

struct empty1
{};

struct empty2
{};

BOOST_HOF_TEST_CASE()
{
    static_assert(boost::hof::detail::is_default_constructible<empty1, empty2>::value, "Not default constructible");
}

BOOST_HOF_TEST_CASE()
{
    static constexpr auto p = boost::hof::pack_basic(empty1());
    BOOST_HOF_TEST_CHECK(p(boost::hof::always(0)) == 0);
    BOOST_HOF_STATIC_TEST_CHECK(p(boost::hof::always(0)) == 0);
#ifndef _MSC_VER
    static_assert(std::is_empty<decltype(p)>::value, "Pack not empty");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(p)>::value, "Not default constructible");

}

BOOST_HOF_TEST_CASE()
{
    static constexpr auto p = boost::hof::pack_basic(empty1(), empty2());
    BOOST_HOF_TEST_CHECK(p(boost::hof::always(0)) == 0);
    BOOST_HOF_STATIC_TEST_CHECK(p(boost::hof::always(0)) == 0);
#ifndef _MSC_VER
    static_assert(std::is_empty<decltype(p)>::value, "Pack not empty");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(p)>::value, "Not default constructible");
}

BOOST_HOF_TEST_CASE()
{
    static constexpr auto p = boost::hof::pack_basic(boost::hof::pack_basic(), boost::hof::pack_basic());
    BOOST_HOF_TEST_CHECK(p(boost::hof::always(0)) == 0);
    BOOST_HOF_STATIC_TEST_CHECK(p(boost::hof::always(0)) == 0);
#ifndef _MSC_VER
    static_assert(std::is_empty<decltype(p)>::value, "Pack not empty");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(p)>::value, "Not default constructible");
}

BOOST_HOF_TEST_CASE()
{
    static constexpr auto p = boost::hof::pack_basic(empty1(), empty2(), empty1());
    BOOST_HOF_TEST_CHECK(p(boost::hof::always(0)) == 0);
    BOOST_HOF_STATIC_TEST_CHECK(p(boost::hof::always(0)) == 0);
#ifndef _MSC_VER
    static_assert(std::is_empty<decltype(p)>::value, "Pack not empty");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(p)>::value, "Not default constructible");
}

BOOST_HOF_TEST_CASE()
{
    static constexpr auto p = boost::hof::pack_basic(empty1(), boost::hof::pack_basic(empty1(), empty2()));
    BOOST_HOF_TEST_CHECK(p(boost::hof::always(0)) == 0);
    BOOST_HOF_STATIC_TEST_CHECK(p(boost::hof::always(0)) == 0);
#ifndef _MSC_VER
    static_assert(std::is_empty<decltype(p)>::value, "Pack not empty");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(p)>::value, "Not default constructible");
}

BOOST_HOF_TEST_CASE()
{
    static constexpr auto p = boost::hof::pack_basic(boost::hof::pack_basic(), boost::hof::pack_basic(boost::hof::pack_basic()), empty1(), boost::hof::pack_basic(empty1(), empty2()));
    BOOST_HOF_TEST_CHECK(p(boost::hof::always(0)) == 0);
    BOOST_HOF_STATIC_TEST_CHECK(p(boost::hof::always(0)) == 0);
#ifndef _MSC_VER
    static_assert(std::is_empty<decltype(p)>::value, "Pack not empty");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(p)>::value, "Not default constructible");
}

struct not_default_constructible
{
    int i;
    constexpr not_default_constructible(int x) : i(x)
    {}
};

struct select_i
{
    template<class T>
    constexpr int operator()(T&& x) const
    {
        return x.i;
    } 

    template<class T, class U>
    constexpr int operator()(T&& x, U&& y) const
    {
        return x.i + y.i;
    } 

    template<class T, class U, class V>
    constexpr int operator()(T&& x, U&& y, V&& z) const
    {
        return x.i + y.i + z.i;
    } 
};

BOOST_HOF_TEST_CASE()
{
    static_assert(!boost::hof::detail::is_default_constructible<not_default_constructible>::value, "Default constructible");
    auto p = boost::hof::pack_basic(not_default_constructible(3));
    static_assert(!boost::hof::detail::is_default_constructible<decltype(p)>::value, "Pack default constructible");
    auto p1 = boost::hof::pack_forward(p);
    static_assert(!boost::hof::detail::is_default_constructible<decltype(p1)>::value, "Packs default constructible");
    auto p2 = boost::hof::pack_forward(p, p);
    static_assert(!boost::hof::detail::is_default_constructible<decltype(p2)>::value, "Packs default constructible");
    auto p3 = boost::hof::pack_forward(p, p, p);
    static_assert(!boost::hof::detail::is_default_constructible<decltype(p3)>::value, "Packs default constructible");
    BOOST_HOF_TEST_CHECK(p(select_i()) == 3);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_basic(not_default_constructible(3))(select_i()) == 3);
}

BOOST_HOF_TEST_CASE()
{
    static_assert(!boost::hof::detail::is_default_constructible<not_default_constructible>::value, "Default constructible");
    auto p = boost::hof::pack_basic(not_default_constructible(1), not_default_constructible(2));
    static_assert(!boost::hof::detail::is_default_constructible<decltype(p)>::value, "Pack default constructible");
    auto p1 = boost::hof::pack_forward(p);
    static_assert(!boost::hof::detail::is_default_constructible<decltype(p1)>::value, "Packs default constructible");
    auto p2 = boost::hof::pack_forward(p, p);
    static_assert(!boost::hof::detail::is_default_constructible<decltype(p2)>::value, "Packs default constructible");
    auto p3 = boost::hof::pack_forward(p, p, p);
    static_assert(!boost::hof::detail::is_default_constructible<decltype(p3)>::value, "Packs default constructible");
    BOOST_HOF_TEST_CHECK(p(select_i()) == 3);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_basic(not_default_constructible(1), not_default_constructible(2))(select_i()) == 3);
}

BOOST_HOF_TEST_CASE()
{
    static_assert(!boost::hof::detail::is_default_constructible<not_default_constructible>::value, "Default constructible");
    auto p = boost::hof::pack_basic(not_default_constructible(1), not_default_constructible(1), not_default_constructible(1));
    static_assert(!boost::hof::detail::is_default_constructible<decltype(p)>::value, "Pack default constructible");
    auto p1 = boost::hof::pack_forward(p);
    static_assert(!boost::hof::detail::is_default_constructible<decltype(p1)>::value, "Packs default constructible");
    auto p2 = boost::hof::pack_forward(p, p);
    static_assert(!boost::hof::detail::is_default_constructible<decltype(p2)>::value, "Packs default constructible");
    auto p3 = boost::hof::pack_forward(p, p, p);
    static_assert(!boost::hof::detail::is_default_constructible<decltype(p3)>::value, "Packs default constructible");
    BOOST_HOF_TEST_CHECK(p(select_i()) == 3);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::pack_basic(not_default_constructible(1), not_default_constructible(1), not_default_constructible(1))(select_i()) == 3);
}


