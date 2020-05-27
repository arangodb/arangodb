// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/functional/apply.hpp>

#include <laws/base.hpp>
#include <support/tracked.hpp>

#include <type_traits>
#include <utility>
namespace hana = boost::hana;


template <int i = 0>
struct nonpod : Tracked {
    nonpod() : Tracked{i} { }
};

struct NonCopyable {
    NonCopyable() = default;
    NonCopyable(NonCopyable const&) = delete;
    NonCopyable& operator=(NonCopyable const&) = delete;
};

struct TestClass {
    explicit TestClass(int x) : data(x) { }
    TestClass(TestClass const&) = delete;
    TestClass& operator=(TestClass const&) = delete;

    int& operator()(NonCopyable&&) & { return data; }
    int const& operator()(NonCopyable&&) const & { return data; }
    int volatile& operator()(NonCopyable&&) volatile & { return data; }
    int const volatile& operator()(NonCopyable&&) const volatile & { return data; }

    int&& operator()(NonCopyable&&) && { return std::move(data); }
    int const&& operator()(NonCopyable&&) const && { return std::move(data); }
    int volatile&& operator()(NonCopyable&&) volatile && { return std::move(data); }
    int const volatile&& operator()(NonCopyable&&) const volatile && { return std::move(data); }

    int data;
};

struct DerivedFromTestClass : TestClass {
    explicit DerivedFromTestClass(int x) : TestClass(x) { }
};


template <typename Signature,  typename Expect, typename Functor>
void test_b12(Functor&& f) {
    // Create the callable object.
    using ClassFunc = Signature TestClass::*;
    ClassFunc func_ptr = &TestClass::operator();

    // Create the dummy arg.
    NonCopyable arg;

    // Check that the deduced return type of invoke is what is expected.
    using DeducedReturnType = decltype(
        hana::apply(func_ptr, std::forward<Functor>(f), std::move(arg))
    );
    static_assert(std::is_same<DeducedReturnType, Expect>::value, "");

    // Check that result_of_t matches Expect.
    using ResultOfReturnType = typename std::result_of<ClassFunc&&(Functor&&, NonCopyable&&)>::type;
    static_assert(std::is_same<ResultOfReturnType, Expect>::value, "");

    // Run invoke and check the return value.
    DeducedReturnType ret = hana::apply(func_ptr, std::forward<Functor>(f), std::move(arg));
    BOOST_HANA_RUNTIME_CHECK(ret == 42);
}

template <typename Expect, typename Functor>
void test_b34(Functor&& f) {
    // Create the callable object.
    using ClassFunc = int TestClass::*;
    ClassFunc func_ptr = &TestClass::data;

    // Check that the deduced return type of invoke is what is expected.
    using DeducedReturnType = decltype(
        hana::apply(func_ptr, std::forward<Functor>(f))
    );
    static_assert(std::is_same<DeducedReturnType, Expect>::value, "");

    // Check that result_of_t matches Expect.
    using ResultOfReturnType = typename std::result_of<ClassFunc&&(Functor&&)>::type;
    static_assert(std::is_same<ResultOfReturnType, Expect>::value, "");

    // Run invoke and check the return value.
    DeducedReturnType ret = hana::apply(func_ptr, std::forward<Functor>(f));
    BOOST_HANA_RUNTIME_CHECK(ret == 42);
}

template <typename Expect, typename Functor>
void test_b5(Functor&& f) {
    NonCopyable arg;

    // Check that the deduced return type of invoke is what is expected.
    using DeducedReturnType = decltype(
        hana::apply(std::forward<Functor>(f), std::move(arg))
    );
    static_assert(std::is_same<DeducedReturnType, Expect>::value, "");

    // Check that result_of_t matches Expect.
    using ResultOfReturnType = typename std::result_of<Functor&&(NonCopyable&&)>::type;
    static_assert(std::is_same<ResultOfReturnType, Expect>::value, "");

    // Run invoke and check the return value.
    DeducedReturnType ret = hana::apply(std::forward<Functor>(f), std::move(arg));
    BOOST_HANA_RUNTIME_CHECK(ret == 42);
}

int& foo(NonCopyable&&) {
    static int data = 42;
    return data;
}

int main() {
    // Test normal usage with a function object
    {
        hana::test::_injection<0> f{};
        using hana::test::ct_eq;

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::apply(f),
            f()
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::apply(f, ct_eq<0>{}),
            f(ct_eq<0>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::apply(f, ct_eq<0>{}, ct_eq<1>{}),
            f(ct_eq<0>{}, ct_eq<1>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::apply(f, ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
            f(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::apply(f, ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
            f(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
        ));

        // Make sure we can use apply with non-PODs
        hana::apply(f, nonpod<>{});
    }

    // Bullets 1 & 2 in the standard
    {
        TestClass cl(42);
        test_b12<int&(NonCopyable&&) &, int&>(cl);
        test_b12<int const&(NonCopyable&&) const &, int const&>(cl);
        test_b12<int volatile&(NonCopyable&&) volatile &, int volatile&>(cl);
        test_b12<int const volatile&(NonCopyable&&) const volatile &, int const volatile&>(cl);

        test_b12<int&&(NonCopyable&&) &&, int&&>(std::move(cl));
        test_b12<int const&&(NonCopyable&&) const &&, int const&&>(std::move(cl));
        test_b12<int volatile&&(NonCopyable&&) volatile &&, int volatile&&>(std::move(cl));
        test_b12<int const volatile&&(NonCopyable&&) const volatile &&, int const volatile&&>(std::move(cl));
    }
    {
        DerivedFromTestClass cl(42);
        test_b12<int&(NonCopyable&&) &, int&>(cl);
        test_b12<int const&(NonCopyable&&) const &, int const&>(cl);
        test_b12<int volatile&(NonCopyable&&) volatile &, int volatile&>(cl);
        test_b12<int const volatile&(NonCopyable&&) const volatile &, int const volatile&>(cl);

        test_b12<int&&(NonCopyable&&) &&, int&&>(std::move(cl));
        test_b12<int const&&(NonCopyable&&) const &&, int const&&>(std::move(cl));
        test_b12<int volatile&&(NonCopyable&&) volatile &&, int volatile&&>(std::move(cl));
        test_b12<int const volatile&&(NonCopyable&&) const volatile &&, int const volatile&&>(std::move(cl));
    }
    {
        TestClass cl_obj(42);
        TestClass *cl = &cl_obj;
        test_b12<int&(NonCopyable&&) &, int&>(cl);
        test_b12<int const&(NonCopyable&&) const &, int const&>(cl);
        test_b12<int volatile&(NonCopyable&&) volatile &, int volatile&>(cl);
        test_b12<int const volatile&(NonCopyable&&) const volatile &, int const volatile&>(cl);
    }
    {
        DerivedFromTestClass cl_obj(42);
        DerivedFromTestClass *cl = &cl_obj;
        test_b12<int&(NonCopyable&&) &, int&>(cl);
        test_b12<int const&(NonCopyable&&) const &, int const&>(cl);
        test_b12<int volatile&(NonCopyable&&) volatile &, int volatile&>(cl);
        test_b12<int const volatile&(NonCopyable&&) const volatile &, int const volatile&>(cl);
    }

    // Bullets 3 & 4 in the standard
    {
        using Fn = TestClass;
        Fn cl(42);
        test_b34<int&>(cl);
        test_b34<int const&>(static_cast<Fn const&>(cl));
        test_b34<int volatile&>(static_cast<Fn volatile&>(cl));
        test_b34<int const volatile&>(static_cast<Fn const volatile &>(cl));

        test_b34<int&&>(static_cast<Fn &&>(cl));
        test_b34<int const&&>(static_cast<Fn const&&>(cl));
        test_b34<int volatile&&>(static_cast<Fn volatile&&>(cl));
        test_b34<int const volatile&&>(static_cast<Fn const volatile&&>(cl));
    }
    {
        using Fn = DerivedFromTestClass;
        Fn cl(42);
        test_b34<int&>(cl);
        test_b34<int const&>(static_cast<Fn const&>(cl));
        test_b34<int volatile&>(static_cast<Fn volatile&>(cl));
        test_b34<int const volatile&>(static_cast<Fn const volatile &>(cl));

        test_b34<int&&>(static_cast<Fn &&>(cl));
        test_b34<int const&&>(static_cast<Fn const&&>(cl));
        test_b34<int volatile&&>(static_cast<Fn volatile&&>(cl));
        test_b34<int const volatile&&>(static_cast<Fn const volatile&&>(cl));
    }
    {
        using Fn = TestClass;
        Fn cl_obj(42);
        Fn* cl = &cl_obj;
        test_b34<int&>(cl);
        test_b34<int const&>(static_cast<Fn const*>(cl));
        test_b34<int volatile&>(static_cast<Fn volatile*>(cl));
        test_b34<int const volatile&>(static_cast<Fn const volatile *>(cl));
    }
    {
        using Fn = DerivedFromTestClass;
        Fn cl_obj(42);
        Fn* cl = &cl_obj;
        test_b34<int&>(cl);
        test_b34<int const&>(static_cast<Fn const*>(cl));
        test_b34<int volatile&>(static_cast<Fn volatile*>(cl));
        test_b34<int const volatile&>(static_cast<Fn const volatile *>(cl));
    }

    // Bullet 5 in the standard
    using FooType = int&(NonCopyable&&);
    {
        FooType& fn = foo;
        test_b5<int &>(fn);
    }
    {
        FooType* fn = foo;
        test_b5<int &>(fn);
    }
    {
        using Fn = TestClass;
        Fn cl(42);
        test_b5<int&>(cl);
        test_b5<int const&>(static_cast<Fn const&>(cl));
        test_b5<int volatile&>(static_cast<Fn volatile&>(cl));
        test_b5<int const volatile&>(static_cast<Fn const volatile &>(cl));

        test_b5<int&&>(static_cast<Fn &&>(cl));
        test_b5<int const&&>(static_cast<Fn const&&>(cl));
        test_b5<int volatile&&>(static_cast<Fn volatile&&>(cl));
        test_b5<int const volatile&&>(static_cast<Fn const volatile&&>(cl));
    }
}
