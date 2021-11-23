// Copyright 2016-2021 Antony Polukhin

// Distributed under the Boost Software License, Version 1.0.
// (See the accompanying file LICENSE_1_0.txt
// or a copy at <http://www.boost.org/LICENSE_1_0.txt>.)

//[pfr_sample_printing
/*`
    The following example shows how to write your own io-manipulator for printing:
*/
#include <boost/pfr/ops.hpp>
#include <ostream>

namespace my_ns {

/// Usage:
///     struct foo {std::uint8_t a, b;};
///     ...
///     std::cout << my_ns::my_io(foo{42, 22});
///
/// Output: 42, 22
template <class T>
auto my_io(const T& value);

namespace detail {
    // Helpers to print individual values
    template <class T>
    void print_each(std::ostream& out, const T& v) { out << v; }
    void print_each(std::ostream& out, std::uint8_t v) { out << static_cast<unsigned>(v); }
    void print_each(std::ostream& out, std::int8_t v) { out << static_cast<int>(v); }

    // Structure to keep a reference to value, that will be ostreamed lower
    template <class T>
    struct io_reference {
        const T& value;
    };

    // Output each field of io_reference::value
    template <class T>
    std::ostream& operator<<(std::ostream& out, io_reference<T>&& x) {
        const char* sep = "";

        boost::pfr::for_each_field(x.value, [&](const auto& v) {
            out << std::exchange(sep, ", ");
            detail::print_each(out, v);
        });
        return out;
    }
}

// Definition:
template <class T>
auto my_io(const T& value) {
    return detail::io_reference<T>{value};
}

} // namespace my_ns
//] [/pfr_sample_printing]


#include <iostream>
#include <sstream>

int main() {
    struct foo {std::uint8_t a, b;};

    std::ostringstream oss;
    oss << my_ns::my_io(foo{42, 22});

    if (oss.str() != "42, 22") {
        return 1;
    }

    struct two_big_strings {
        std::string first;
        std::string second;
    };

#if BOOST_PFR_USE_CPP17 || BOOST_PFR_USE_LOOPHOLE
    const char* huge_string = "Some huge string that should not fit into std::string SSO."
        "And by 'huge' I mean really HUGE string with multiple statements and a lot of periods........."
    ;

    oss.str({});
    oss << my_ns::my_io(two_big_strings{
        huge_string, huge_string
    });

    if (oss.str() != huge_string + std::string(", ") + huge_string) {
        return 2;
    }
#endif

    return 0;
}
