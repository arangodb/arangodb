/*<-
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)
->*/
//[ hideous_template
template<typename T> struct foo;

//function type without varargs
template<class Return, class... Args> struct foo<Return(Args...)> {};
template<class Return, class... Args> struct foo<Return(Args...) &> {};
template<class Return, class... Args> struct foo<Return(Args...) &&> {};
template<class Return, class... Args> struct foo<Return(Args...) const> {};
template<class Return, class... Args> struct foo<Return(Args...) const &> {};
template<class Return, class... Args> struct foo<Return(Args...) const &&> {};
template<class Return, class... Args> struct foo<Return(Args...) volatile> {};
template<class Return, class... Args> struct foo<Return(Args...) volatile &> {};
template<class Return, class... Args> struct foo<Return(Args...) volatile &&> {};
template<class Return, class... Args> struct foo<Return(Args...) const volatile> {};
template<class Return, class... Args> struct foo<Return(Args...) const volatile &> {};
template<class Return, class... Args> struct foo<Return(Args...) const volatile &&> {};

//function type with varargs
template<class Return, class... Args> struct foo<Return(Args..., ...)> {};
template<class Return, class... Args> struct foo<Return(Args..., ...) &> {};
template<class Return, class... Args> struct foo<Return(Args..., ...) &&> {};
template<class Return, class... Args> struct foo<Return(Args..., ...) const> {};
template<class Return, class... Args> struct foo<Return(Args..., ...) const &> {};
template<class Return, class... Args> struct foo<Return(Args..., ...) const &&> {};
template<class Return, class... Args> struct foo<Return(Args..., ...) volatile> {};
template<class Return, class... Args> struct foo<Return(Args..., ...) volatile &> {};
template<class Return, class... Args> struct foo<Return(Args..., ...) volatile &&> {};
template<class Return, class... Args> struct foo<Return(Args..., ...) const volatile> {};
template<class Return, class... Args> struct foo<Return(Args..., ...) const volatile &> {};
template<class Return, class... Args> struct foo<Return(Args..., ...) const volatile &&> {};

//]
