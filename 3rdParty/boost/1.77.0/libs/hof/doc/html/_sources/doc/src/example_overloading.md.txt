<!-- Copyright 2018 Paul Fultz II
     Distributed under the Boost Software License, Version 1.0.
     (http://www.boost.org/LICENSE_1_0.txt)
-->

Conditional overloading
=======================

Conditional overloading takes a set of functions and calls the first one that is callable. This is one of the ways to resolve ambiguity with overloads, but more importantly it allows an alternative function to be used when the first is not callable.

Stringify
---------

Take a look at this example of defining a `stringify` function from
stackoverflow [here](http://stackoverflow.com/questions/30189926/metaprograming-failure-of-function-definition-defines-a-separate-function/30515874).

The user would like to write `stringify` to call `to_string` where applicable
and fallback on using `sstream` to convert to a string. Most of the top
answers usually involve some amount of metaprogramming using either `void_t`
or `is_detected`(see [n4502](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4502.pdf)): 

    template<class T>
    using to_string_t = decltype(std::to_string(std::declval<T>()));

    template<class T>
    using has_to_string = std::experimental::is_detected<to_string_t, T>;

    template<typename T> 
    typename std::enable_if<has_to_string<T>{}, std::string>::type 
    stringify(T t)
    {
        return std::to_string(t);
    }
    template<typename T> 
    typename std::enable_if<!has_to_string<T>{}, std::string>::type 
    stringify(T t)
    {
        return static_cast<std::ostringstream&>(std::ostringstream() << t).str();
    }

However, with Boost.HigherOrderFunctions it can simply be written like
this:

    BOOST_HOF_STATIC_LAMBDA_FUNCTION(stringify) = first_of(
        [](auto x) BOOST_HOF_RETURNS(std::to_string(x)),
        [](auto x) BOOST_HOF_RETURNS(static_cast<std::ostringstream&>(std::ostringstream() << x).str())
    );

So, using [`BOOST_HOF_RETURNS`](/include/boost/hof/returns) not only deduces the return type for the function, but it also constrains the function on whether the expression is valid or not. So by writing `BOOST_HOF_RETURNS(std::to_string(x))` then the first function will try to call `std::to_string` function if possible. If not, then the second function will be called. 

The second function still uses [`BOOST_HOF_RETURNS`](/include/boost/hof/returns), so the function will still be constrained by whether the `<<` stream operator can be used. Although it may seem unnecessary because there is not another function, however, this makes the function composable. So we could use this to define a `serialize` function that tries to call `stringify` first, otherwise it looks for the member `.serialize()`:

    BOOST_HOF_STATIC_LAMBDA_FUNCTION(serialize) = first_of(
        [](auto x) BOOST_HOF_RETURNS(stringify(x)),
        [](auto x) BOOST_HOF_RETURNS(x.serialize())
    );

static_if
---------

In addition, this can be used with the [`boost::hof::if_`](/include/boost/hof/if) decorator to create `static_if`-like
constructs on pre-C++17 compilers. For example, Baptiste Wicht discusses how one could write `static_if` in C++ [here](http://baptiste-wicht.com/posts/2015/07/simulate-static_if-with-c11c14.html).

He wants to be able to write this:

    template<typename T>
    void decrement_kindof(T& value){
        if constexpr(std::is_same<std::string, T>()){
            value.pop_back();
        } else {
            --value;
        }
    }

However, that isn't possible before C++17. With Boost.HigherOrderFunctions one can simply write
this:

    template<typename T>
    void decrement_kindof(T& value)
    {
        eval(first_of(
            if_(std::is_same<std::string, T>())([&](auto id){
                id(value).pop_back();
            }),
            [&](auto id){
                --id(value);
            }
        ));
    }

The `id` parameter passed to the lambda is the [`identity`](/include/boost/hof/identity) function. As explained in the article, this is used to delay the lookup of types by making it a dependent type(i.e. the type depends on a template parameter), which is necessary to avoid compile errors. The [`eval`](/include/boost/hof/eval) function that is called will pass this `identity` function to the lambdas.

The advantage of using Boost.HigherOrderFunctions instead of the solution in Baptiste
Wicht's blog, is that [`first_of`](/include/boost/hof/conditional) allows more than just two conditions. So if
there was another trait to be checked, such as `is_stack`, it could be written
like this:

    template<typename T>
    void decrement_kindof(T& value)
    {
        eval(first_of(
            if_(is_stack<T>())([&](auto id){
                id(value).pop();
            }),
            if_(std::is_same<std::string, T>())([&](auto id){
                id(value).pop_back();
            }),
            [&](auto id){
                --id(value);
            }
        ));
    }

Type traits
-----------

Furthermore, this technique can be used to write type traits as well. Jens
Weller was looking for a way to define a general purpose detection for pointer
operands(such as `*` and `->`). One way to accomplish this is like
this:

    // Check that T has member function for operator* and ope
    template<class T>
    auto has_pointer_member(const T&) -> decltype(
        &T::operator*,
        &T::operator->,
        std::true_type{}
    );

    BOOST_HOF_STATIC_LAMBDA_FUNCTION(has_pointer_operators) = first_of(
        BOOST_HOF_LIFT(has_pointer_member),
        [](auto* x) -> bool_constant<(!std::is_void<decltype(*x)>())> { return {}; },
        always(std::false_type{})
    );

    template<class T>
    struct is_dereferenceable
    : decltype(has_pointer_operators(std::declval<T>()))
    {};

Which is much simpler than the other implementations that were written, which were
about 3 times the amount of code(see [here](https://gist.github.com/lefticus/6fdccb18084a1a3410d5)).

The `has_pointer_operators` function works by first trying to call `has_pointer_member` which returns `true_type` if the type has member functions `operator*` and `operator->`, otherwise the function is not callable. The next function is only callable on pointers, which returns true if it is not a `void*` pointer(because `void*` pointers are not dereferenceable). Finally, if none of those functions matched then the last function will always return `false_type`. 
