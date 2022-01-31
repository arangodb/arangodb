<!-- Copyright 2018 Paul Fultz II
     Distributed under the Boost Software License, Version 1.0.
     (http://www.boost.org/LICENSE_1_0.txt)
-->

Point-free style programming
============================

[Point-free style](https://en.wikipedia.org/wiki/Tacit_programming) programing(or tacit programming) is a style where the arguments to the function are not explicity defined. Rather, the function is defined as the composition of other functions where function adaptors manipulate the function arguments. The advantage of using point-free style in C++ is the template machinery involved with function arguments can be avoided.

Variadic print
--------------

For example, if we want to write a variadic print function that prints each argument, like this:

    print("Hello", "World");

We would write something like the following, which would recursively iterate over the arguments using varidiac templates:

    // Base case
    void print()
    {}

    template<class T, class... Ts>
    void print(const T& x, const Ts&... xs)
    {
        std::cout << x;
        print(xs...);
    }

Instead with point-free style, we can write this using the [`proj`](/include/boost/hof/by) adaptor, which calls a function on each arguments. Of course, `std::cout` is not a function, but we can make it one by using `BOOST_HOF_LIFT`:

    BOOST_HOF_STATIC_FUNCTION(simple_print) = BOOST_HOF_LIFT(std::ref(std::cout) << _);

This uses the [placeholders](/include/boost/hof/placeholders) to create a function that prints to `std::cout`. Then we can pass `simple_print` to the [`proj`](/include/boost/hof/by) adaptor:

    BOOST_HOF_STATIC_FUNCTION(print) = proj(simple_print);

As the [`proj`](/include/boost/hof/by) adaptor calls the function for each argument passed in, `b(f)(x, y)` is the equivalent of calling `f(x)` and then `f(y)`. In this case, it will call `simple_print(x)` and then `simple_print(y)`:

    print("Hello", "World");

Which prints out:

    HelloWorld

Of course, this puts all the output together, but we can further extend this to print a new line for each item by composing it:

    BOOST_HOF_STATIC_FUNCTION(print_lines) = proj(flow(simple_print, _ << std::integral_constant<char, '\n'>{}));

The [flow](/include/boost/hof/flow) adaptor does function composition but the functions are called from left-to-right. That is `flow(f, g)(x)` is equivalent to `g(f(x))`. So in this case, it will call `simple_print` on the argument which returns `std::cout` and then pass that to the next function which calls the stream with the newline character. In the above, we write `std::integral_constant<char, '\n'>{}` instead of just `'\n'` because the function is statically defined, so all values must be defined statically.

So now calling `print_lines`:

    print_lines("Hello", "World");

It will print out:

    Hello
    World

With each argument on its own line.

Variadic sum
------------

Another example, say we would like to write a varidiac version of `max`. We could implement it like this:

    // Base case
    template<class T>
    T max(const T& x)
    {
        return x;
    }

    template<class T, class... Ts>
    T max(const T& x, const T& y, const Ts&... xs)
    {
        return std::max(x, max(y, xs...));
    }

With point-free style programming, we can recognize this is a [fold](https://en.wikipedia.org/wiki/Fold_%28higher-order_function%29), and write it using the [`fold`](/include/boost/hof/fold) adaptor, which will do a fold over the variadic parameters:

    BOOST_HOF_STATIC_FUNCTION(max) = fold(BOOST_HOF_LIFT(std::max));

[`BOOST_HOF_LIFT`](/include/boost/hof/lift) is used to grab the entire overload set of `std::max` function, which is needed since `std::max` is templated and we want the variadic `std::max` function to handle any types as well. So now it can be called like this:

    auto n = max(1, 2, 4, 3); // Returns 4
    auto m = max(0.1, 0.2, 0.5, 0.4); // Returns 0.5

By using [`fold`](/include/boost/hof/fold), `max(1, 2, 4, 3)` will call `std::max` like `std::max(std::max(std::max(1, 2), 4), 3)` and `max(0.1, 0.2, 0.5, 0.4)` will be called like `std::max(std::max(std::max(0.1, 0.2), 0.5), 0.4)`.

