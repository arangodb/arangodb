// Copyright 2016-2021 Antony Polukhin

// Distributed under the Boost Software License, Version 1.0.
// (See the accompanying file LICENSE_1_0.txt
// or a copy at <http://www.boost.org/LICENSE_1_0.txt>.)

#include <cassert>
#include <iostream>
#include <unordered_set>
#include <set>

#include <boost/pfr.hpp>
#include <boost/type_index.hpp>

// boost-no-inspect
void test_examples() {

#if BOOST_PFR_USE_CPP17
  {
//[pfr_quick_examples_ops
    // Assert equality.
    // Note that the equality operator for structure is not defined.

    struct test {
        std::string f1;
        std::string_view f2;
    };

    assert(
        boost::pfr::eq(test{"aaa", "zomg"}, test{"aaa", "zomg"})
    );
//]
  }
#endif

  {
//[pfr_quick_examples_for_each
    // Increment each field of the variable on 1 and
    // output the content of the variable.

    struct test {
        int f1;
        long f2;
    };

    test var{42, 43};

    boost::pfr::for_each_field(var, [](auto& field) {
        field += 1;
    });

    // Outputs: {43, 44}
    std::cout << boost::pfr::io(var);
//]
  }

  {
//[pfr_quick_examples_for_each_idx
    // Iterate over fields of a variable and output index and
    // type of a variable.

    struct tag0{};
    struct tag1{};
    struct sample {
        tag0 a;
        tag1 b;
    };

    // Outputs:
    //  0: tag0
    //  1: tag1
    boost::pfr::for_each_field(sample{}, [](const auto& field, std::size_t idx) {
        std::cout << '\n' << idx << ": "
            << boost::typeindex::type_id_runtime(field);
    });
//]
  }


  {
//[pfr_quick_examples_tuple_size
    // Getting fields count of some structure

    struct some { int a,b,c,d,e; };

    std::cout << "Fields count in structure: "
        << boost::pfr::tuple_size<some>::value  // Outputs: 5
        << '\n';
//]
  }

  {
//[pfr_quick_examples_get
    // Get field by index and assign new value to that field

    struct sample {
        char c;
        float f;
    };

    sample var{};
    boost::pfr::get<1>(var) = 42.01f;

    std::cout << var.f; // Outputs: 42.01
//]
  }

#if BOOST_PFR_USE_CPP17 || BOOST_PFR_USE_LOOPHOLE
  {
//[pfr_quick_examples_structure_to_tuple
    // Getting a std::tuple of values from structures fields

    struct foo { int a, b; };
    struct other {
        char c;
        foo nested;
    };

    other var{'A', {3, 4}};
    std::tuple<char, foo> t = boost::pfr::structure_to_tuple(var);
    assert(std::get<0>(t) == 'A');
    assert(
        boost::pfr::eq(std::get<1>(t), foo{3, 4})
    );
//]
  }
#endif

#if BOOST_PFR_USE_CPP17 || BOOST_PFR_USE_LOOPHOLE
  {
//[pfr_quick_examples_structure_tie
    // Getting a std::tuple of references to structure fields

    struct foo { int a, b; };
    struct other {
        char c;
        foo f;
    };

    other var{'A', {14, 15}};
    std::tuple<char&, foo&> t = boost::pfr::structure_tie(var);
    std::get<1>(t) = foo{1, 2};

    std::cout << boost::pfr::io(var.f); // Outputs: {1, 2}
//]
  }
#endif

} // void test_examples()

//[pfr_quick_examples_functions_for
// Define all the comparison and IO operators for my_structure type along
// with hash_value function.

#include <boost/pfr/functions_for.hpp>

namespace my_namespace {
    struct my_structure {
        int a,b,c,d,e,f,g;
        // ...
    };
    BOOST_PFR_FUNCTIONS_FOR(my_structure)
}
//]

//[pfr_quick_examples_eq_fields
// Define only the equality and inequality operators for my_eq_ne_structure.

#include <boost/pfr/functions_for.hpp>

namespace my_namespace {
    struct my_eq_ne_structure {
        float a,b,c,d,e,f,g;
        // ...
    };

    inline bool operator==(const my_eq_ne_structure& x, const my_eq_ne_structure& y) {
        return boost::pfr::eq_fields(x, y);
    }

    inline bool operator!=(const my_eq_ne_structure& x, const my_eq_ne_structure& y) {
        return boost::pfr::ne_fields(x, y);
    }
}
//]

int main() {
    test_examples();
}
