// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/at.hpp>
#include <boost/hana/at_key.hpp>
#include <boost/hana/concept/sequence.hpp>
#include <boost/hana/concept/struct.hpp>
#include <boost/hana/define_struct.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/keys.hpp>
#include <boost/hana/length.hpp>
#include <boost/hana/not_equal.hpp>
#include <boost/hana/tuple.hpp>

#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>
namespace hana = boost::hana;


struct Car {
    BOOST_HANA_DEFINE_STRUCT(Car,
        (std::string, brand),
        (std::string, model)
    );
};

struct Person {
    BOOST_HANA_DEFINE_STRUCT(Person,
        (std::string, name),
        (std::string, last_name),
        (int, age)
    );
};

template <typename T>
  std::enable_if_t<std::is_same<T, int>::value,
T> from_json(std::istream& in) {
    T result;
    in >> result;
    return result;
}

template <typename T>
  std::enable_if_t<std::is_same<T, std::string>::value,
T> from_json(std::istream& in) {
    char quote;
    in >> quote;

    T result;
    char c;
    while (in.get(c) && c != '"') {
        result += c;
    }
    return result;
}


template <typename T>
  std::enable_if_t<hana::Struct<T>::value,
T> from_json(std::istream& in) {
    T result;
    char brace;
    in >> brace;

    // CAREFUL: This only works if the input JSON contains the fields in the
    //          same order as the struct declares them.
    hana::for_each(hana::keys(result), [&](auto key) {
        in.ignore(std::numeric_limits<std::streamsize>::max(), ':');
        auto& member = hana::at_key(result, key);
        using Member = std::remove_reference_t<decltype(member)>;
        member = from_json<Member>(in);
    });
    in >> brace;
    return result;
}

template <typename Xs>
  std::enable_if_t<hana::Sequence<Xs>::value,
Xs> from_json(std::istream& in) {
    Xs result;
    char bracket;
    in >> bracket;
    hana::length(result).times.with_index([&](auto i) {
        if (i != 0u) {
            char comma;
            in >> comma;
        }

        auto& element = hana::at(result, i);
        using Element = std::remove_reference_t<decltype(element)>;
        element = from_json<Element>(in);
    });
    in >> bracket;
    return result;
}


int main() {
    std::istringstream json(R"EOS(
        [
            {
                "name": "John",
                "last_name": "Doe",
                "age": 30
            },
            {
                "brand": "BMW",
                "model": "Z3"
            },
            {
                "brand": "Audi",
                "model": "A4"
            }
        ]
    )EOS");

    auto actual = from_json<hana::tuple<Person, Car, Car>>(json);

    auto expected = hana::make_tuple(Person{"John", "Doe", 30},
                                     Car{"BMW", "Z3"},
                                     Car{"Audi", "A4"});

    BOOST_HANA_RUNTIME_CHECK(actual == expected);
}
