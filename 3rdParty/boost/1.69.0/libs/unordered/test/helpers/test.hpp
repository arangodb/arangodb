
// Copyright 2006-2009 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#if !defined(BOOST_UNORDERED_TEST_TEST_HEADER)
#define BOOST_UNORDERED_TEST_TEST_HEADER

#include <boost/core/lightweight_test.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/stringize.hpp>

#define UNORDERED_AUTO_TEST(x)                                                 \
  struct BOOST_PP_CAT(x, _type) : public ::test::registered_test_base          \
  {                                                                            \
    BOOST_PP_CAT(x, _type)                                                     \
    () : ::test::registered_test_base(BOOST_PP_STRINGIZE(x))                   \
    {                                                                          \
      ::test::get_state().add_test(this);                                      \
    }                                                                          \
    void run();                                                                \
  };                                                                           \
  BOOST_PP_CAT(x, _type) x;                                                    \
  void BOOST_PP_CAT(x, _type)::run()

#define RUN_TESTS()                                                            \
  int main(int, char**)                                                        \
  {                                                                            \
    BOOST_UNORDERED_TEST_COMPILER_INFO()                                       \
    ::test::get_state().run_tests();                                           \
    return boost::report_errors();                                             \
  }

#define RUN_TESTS_QUIET()                                                      \
  int main(int, char**)                                                        \
  {                                                                            \
    BOOST_UNORDERED_TEST_COMPILER_INFO()                                       \
    ::test::get_state().run_tests(true);                                       \
    return boost::report_errors();                                             \
  }

#define UNORDERED_SUB_TEST(x)                                                  \
  for (int UNORDERED_SUB_TEST_VALUE = ::test::get_state().start_sub_test(x);   \
       UNORDERED_SUB_TEST_VALUE;                                               \
       UNORDERED_SUB_TEST_VALUE =                                              \
         ::test::get_state().end_sub_test(x, UNORDERED_SUB_TEST_VALUE))

namespace test {

  struct registered_test_base
  {
    registered_test_base* next;
    char const* name;
    explicit registered_test_base(char const* n) : name(n) {}
    virtual void run() = 0;
    virtual ~registered_test_base() {}
  };

  struct state
  {
    bool is_quiet;
    registered_test_base* first_test;
    registered_test_base* last_test;

    state() : is_quiet(false), first_test(0), last_test(0) {}

    void add_test(registered_test_base* test)
    {
      if (last_test) {
        last_test->next = test;
      } else {
        first_test = test;
      }
      last_test = test;
    }

    void run_tests(bool quiet = false)
    {
      is_quiet = quiet;

      for (registered_test_base* i = first_test; i; i = i->next) {
        int error_count = boost::detail::test_errors();
        if (!quiet) {
          BOOST_LIGHTWEIGHT_TEST_OSTREAM << "Running " << i->name << "\n"
                                         << std::flush;
        }
        i->run();
        BOOST_LIGHTWEIGHT_TEST_OSTREAM << std::flush;
        if (quiet && error_count != boost::detail::test_errors()) {
          BOOST_LIGHTWEIGHT_TEST_OSTREAM << "Error in: " << i->name << "\n"
                                         << std::flush;
        }
      }
    }

    int start_sub_test(char const* name)
    {
      if (!is_quiet) {
        BOOST_LIGHTWEIGHT_TEST_OSTREAM << "Sub-test: " << name << "\n"
                                       << std::flush;
      }
      // Add one because it's used as a loop condition.
      return boost::detail::test_errors() + 1;
    }

    int end_sub_test(char const* name, int value)
    {
      if (is_quiet && value != boost::detail::test_errors() + 1) {
        BOOST_LIGHTWEIGHT_TEST_OSTREAM << "Error in sub-test: " << name << "\n"
                                       << std::flush;
      }
      return 0;
    }
  };

  // Get the currnet translation unit's test state.
  static inline state& get_state()
  {
    static state instance;
    return instance;
  }
}

#if defined(__cplusplus)
#define BOOST_UNORDERED_CPLUSPLUS __cplusplus
#else
#define BOOST_UNORDERED_CPLUSPLUS "(not defined)"
#endif

#define BOOST_UNORDERED_TEST_COMPILER_INFO()                                   \
  {                                                                            \
    BOOST_LIGHTWEIGHT_TEST_OSTREAM                                             \
      << "Compiler: " << BOOST_COMPILER << "\n"                                \
      << "Library: " << BOOST_STDLIB << "\n"                                   \
      << "__cplusplus: " << BOOST_UNORDERED_CPLUSPLUS << "\n\n"                \
      << "BOOST_UNORDERED_HAVE_PIECEWISE_CONSTRUCT: "                          \
      << BOOST_UNORDERED_HAVE_PIECEWISE_CONSTRUCT << "\n"                      \
      << "BOOST_UNORDERED_EMPLACE_LIMIT: " << BOOST_UNORDERED_EMPLACE_LIMIT    \
      << "\n"                                                                  \
      << "BOOST_UNORDERED_USE_ALLOCATOR_TRAITS: "                              \
      << BOOST_UNORDERED_USE_ALLOCATOR_TRAITS << "\n"                          \
      << "BOOST_UNORDERED_CXX11_CONSTRUCTION: "                                \
      << BOOST_UNORDERED_CXX11_CONSTRUCTION << "\n\n"                          \
      << std::flush;                                                           \
  }

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/seq/fold_left.hpp>
#include <boost/preprocessor/seq/for_each_product.hpp>
#include <boost/preprocessor/seq/seq.hpp>
#include <boost/preprocessor/seq/to_tuple.hpp>

// Run test with every combination of the parameters (a sequence of sequences)
#define UNORDERED_TEST(name, parameters)                                       \
  BOOST_PP_SEQ_FOR_EACH_PRODUCT(UNORDERED_TEST_OP, ((name))((1))parameters)

#define UNORDERED_TEST_REPEAT(name, n, parameters)                             \
  BOOST_PP_SEQ_FOR_EACH_PRODUCT(UNORDERED_TEST_OP, ((name))((n))parameters)

#define UNORDERED_TEST_OP(r, product)                                          \
  UNORDERED_TEST_OP2(BOOST_PP_SEQ_ELEM(0, product),                            \
    BOOST_PP_SEQ_ELEM(1, product),                                             \
    BOOST_PP_SEQ_TAIL(BOOST_PP_SEQ_TAIL(product)))

#define UNORDERED_TEST_OP2(name, n, params)                                    \
  UNORDERED_AUTO_TEST (                                                        \
    BOOST_PP_SEQ_FOLD_LEFT(UNORDERED_TEST_OP_JOIN, name, params)) {            \
    for (int i = 0; i < n; ++i)                                                \
      name BOOST_PP_SEQ_TO_TUPLE(params);                                      \
  }

#define UNORDERED_TEST_OP_JOIN(s, state, elem)                                 \
  BOOST_PP_CAT(state, BOOST_PP_CAT(_, elem))

#define UNORDERED_MULTI_TEST(name, impl, parameters)                           \
  UNORDERED_MULTI_TEST_REPEAT(name, impl, 1, parameters)

#define UNORDERED_MULTI_TEST_REPEAT(name, impl, n, parameters)                 \
  UNORDERED_AUTO_TEST (name) {                                                 \
    BOOST_PP_SEQ_FOR_EACH_PRODUCT(                                             \
      UNORDERED_MULTI_TEST_OP, ((impl))((n))parameters)                        \
  }

#define UNORDERED_MULTI_TEST_OP(r, product)                                    \
  UNORDERED_MULTI_TEST_OP2(BOOST_PP_SEQ_ELEM(0, product),                      \
    BOOST_PP_SEQ_ELEM(1, product),                                             \
    BOOST_PP_SEQ_TAIL(BOOST_PP_SEQ_TAIL(product)))

// Need to wrap UNORDERED_SUB_TEST in a block to avoid an msvc bug.
// https://support.microsoft.com/en-gb/help/315481/bug-too-many-unnested-loops-incorrectly-causes-a-c1061-compiler-error-in-visual-c
#define UNORDERED_MULTI_TEST_OP2(name, n, params)                              \
  {                                                                            \
    UNORDERED_SUB_TEST(BOOST_PP_STRINGIZE(                                     \
      BOOST_PP_SEQ_FOLD_LEFT(UNORDERED_TEST_OP_JOIN, name, params)))           \
    {                                                                          \
      for (int i = 0; i < n; ++i)                                              \
        name BOOST_PP_SEQ_TO_TUPLE(params);                                    \
    }                                                                          \
  }

#endif
