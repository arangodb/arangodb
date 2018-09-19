#include <tuple>
#include <utility>
#include <type_traits>
#include <boost/callable_traits/has_void_return.hpp>
#include "test.hpp"

struct foo;

template<typename T>
void assert_void_return() {
    CT_ASSERT(has_void_return<T>::value);
}

template<typename T>
void assert_not_void_return() {
    CT_ASSERT(!has_void_return<T>::value);
}

int main() {

    assert_void_return<void()>();
    assert_void_return<void(...)>();

#ifndef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS
    assert_void_return<void(int) const>();
    assert_void_return<void(int) volatile>();
    assert_void_return<void(int) const volatile>();
#endif // #ifndef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS

    assert_void_return<void(foo::*)()>();
    assert_void_return<void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(int, ...)>();
    assert_void_return<void(foo::*)(int) const>();
    assert_void_return<void(foo::*)() volatile>();
    assert_void_return<void(foo::*)(int) const volatile>();

    assert_void_return<void(*)()>();
    assert_void_return<void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC *)(int, ...)>();

    assert_void_return<void(&)(int)>();
    assert_void_return<void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC &)(...)>();

    auto lambda = []{};
    assert_void_return<decltype(lambda)>();

    assert_not_void_return<void>();
    assert_not_void_return<int>();
    assert_not_void_return<void*>();
    assert_not_void_return<void* foo::*>();
    assert_not_void_return<void(** const)()>();
    assert_not_void_return<int()>();
    assert_not_void_return<int(*)()>();
    assert_not_void_return<int(&)()>();
    assert_not_void_return<int(foo::*)()>();
    assert_not_void_return<int(foo::*)() const>();
    assert_not_void_return<int(foo::*)() volatile>();
    assert_not_void_return<int(foo::*)() const volatile>();
    assert_not_void_return<void(foo::**)()>();
}
