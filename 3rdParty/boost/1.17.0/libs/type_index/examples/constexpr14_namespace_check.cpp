// Copyright 2013-2019 Antony Polukhin

// Distributed under the Boost Software License, Version 1.0.
// (See the accompanying file LICENSE_1_0.txt
// or a copy at <http://www.boost.org/LICENSE_1_0.txt>.)

#include <boost/config.hpp>

template <class T>
void do_something(const T&) {}


#if !defined(BOOST_NO_CXX14_CONSTEXPR) && !defined(BOOST_NO_CXX11_CONSTEXPR)
// Implementation of this function is not essential for the example
template <std::size_t N>
constexpr bool starts_with(const char* name, const char (&ns)[N]) noexcept {
    for (std::size_t i = 0; i < N - 1; ++i)
        if (name[i] != ns[i])
            return false;
            
    return true;
}

//[type_index_constexpr14_namespace_example
/*`
    The following example shows that `boost::typeindex::ctti_type_index` is usable at compile time on
    a C++14 compatible compilers.

    In this example we'll create and use a constexpr function that checks namespace of the provided type.
*/

#include <boost/type_index/ctti_type_index.hpp>

// Helper function that returns true if `name` starts with `substr`
template <std::size_t N>
constexpr bool starts_with(const char* name, const char (&substr)[N]) noexcept;


// Function that returns true if `T` declared in namespace `ns`
template <class T, std::size_t N>
constexpr bool in_namespace(const char (&ns)[N]) noexcept {
    const char* name = boost::typeindex::ctti_type_index::type_id<T>().raw_name();

    // Some compilers add `class ` or `struct ` before the namespace, so we need to skip those words first
    if (starts_with(name, "class ")) {
        name += sizeof("class ") - 1;
    } else if (starts_with(name, "struct ")) {
        name += sizeof("struct ") - 1;
    }

    return starts_with(name, ns) && starts_with(name + N - 1, "::");
}

/*`
    Now when we have that wonderfull function, we can do static assertions and other compile-time validations:
*/

namespace my_project {
    struct serializer {
        template <class T>
        void serialize(const T& value) {
            static_assert(
                in_namespace<T>("my_project::types") || in_namespace<T>("my_project::types_ext"),
                "Only types from namespaces `my_project::types` and `my_project::types_ext` are allowed to be serialized using `my_project::serializer`"
            );

            // Actual implementation of the serialization goes below
            // ...
            do_something(value);
        }
    };

    namespace types {
        struct foo{};
        struct bar{};
    }
} // namespace my_project

int main() {
    my_project::serializer s;
    my_project::types::foo f;
    my_project::types::bar b;

    s.serialize(f);
    s.serialize(b);

    // short sh = 0;
    // s.serialize(sh); // Fails the static_assert!
}
//] [/type_index_constexpr14_namespace_example]

#else // #if !defined(BOOST_NO_CXX14_CONSTEXPR) && !defined(BOOST_NO_CXX11_CONSTEXPR)

int main() {}

#endif

