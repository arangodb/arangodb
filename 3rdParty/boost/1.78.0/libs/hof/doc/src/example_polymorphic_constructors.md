<!-- Copyright 2018 Paul Fultz II
     Distributed under the Boost Software License, Version 1.0.
     (http://www.boost.org/LICENSE_1_0.txt)
-->

Polymorphic constructors
========================

Writing polymorphic constructors(such as `make_tuple`) is a boilerplate that
has to be written over and over again for template classes: 

    template <class T>
    struct unwrap_refwrapper
    {
        typedef T type;
    };
     
    template <class T>
    struct unwrap_refwrapper<std::reference_wrapper<T>>
    {
        typedef T& type;
    };
     
    template <class T>
    struct unwrap_ref_decay
    : unwrap_refwrapper<typename std::decay<T>::type>
    {};

    template <class... Types>
    std::tuple<typename unwrap_ref_decay<Types>::type...> make_tuple(Types&&... args)
    {
        return std::tuple<typename unwrap_ref_decay<Types>::type...>(std::forward<Types>(args)...);
    }

The [`construct`](include/boost/hof/construct) function takes care of all this boilerplate, and the above can be simply written like this:

    BOOST_HOF_STATIC_FUNCTION(make_tuple) = construct<std::tuple>();
