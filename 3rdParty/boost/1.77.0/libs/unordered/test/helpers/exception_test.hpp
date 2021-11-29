
// Copyright 2006-2009 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#if !defined(BOOST_UNORDERED_EXCEPTION_TEST_HEADER)
#define BOOST_UNORDERED_EXCEPTION_TEST_HEADER

#include "./count.hpp"
#include "./test.hpp"

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/seq/elem.hpp>
#include <boost/preprocessor/seq/for_each_product.hpp>

#define UNORDERED_EXCEPTION_TEST_CASE(name, test_func, type)                   \
  UNORDERED_AUTO_TEST (name) {                                                 \
    test_func<type> fixture;                                                   \
    ::test::lightweight::exception_safety(                                     \
      fixture, BOOST_STRINGIZE(test_func<type>));                              \
  }

#define UNORDERED_EXCEPTION_TEST_CASE_REPEAT(name, test_func, n, type)         \
  UNORDERED_AUTO_TEST (name) {                                                 \
    for (unsigned i = 0; i < n; ++i) {                                         \
      test_func<type> fixture;                                                 \
      ::test::lightweight::exception_safety(                                   \
        fixture, BOOST_STRINGIZE(test_func<type>));                            \
    }                                                                          \
  }

#define UNORDERED_EPOINT_IMPL ::test::lightweight::epoint

#define UNORDERED_EXCEPTION_TEST_POSTFIX RUN_TESTS()

#define EXCEPTION_TESTS(test_seq, param_seq)                                   \
  BOOST_PP_SEQ_FOR_EACH_PRODUCT(EXCEPTION_TESTS_OP, (test_seq)((1))(param_seq))

#define EXCEPTION_TESTS_REPEAT(n, test_seq, param_seq)                         \
  BOOST_PP_SEQ_FOR_EACH_PRODUCT(EXCEPTION_TESTS_OP, (test_seq)((n))(param_seq))

#define EXCEPTION_TESTS_OP(r, product)                                         \
  UNORDERED_EXCEPTION_TEST_CASE_REPEAT(                                        \
    BOOST_PP_CAT(BOOST_PP_SEQ_ELEM(0, product),                                \
      BOOST_PP_CAT(_, BOOST_PP_SEQ_ELEM(2, product))),                         \
    BOOST_PP_SEQ_ELEM(0, product), BOOST_PP_SEQ_ELEM(1, product),              \
    BOOST_PP_SEQ_ELEM(2, product))

#define UNORDERED_SCOPE(scope_name)                                            \
  for (::test::scope_guard unordered_test_guard(BOOST_STRINGIZE(scope_name));  \
       !unordered_test_guard.dismissed(); unordered_test_guard.dismiss())

#define UNORDERED_EPOINT(name)                                                 \
  if (::test::exceptions_enabled) {                                            \
    UNORDERED_EPOINT_IMPL(name);                                               \
  }

#define ENABLE_EXCEPTIONS                                                      \
  ::test::exceptions_enable BOOST_PP_CAT(ENABLE_EXCEPTIONS_, __LINE__)(true)

#define DISABLE_EXCEPTIONS                                                     \
  ::test::exceptions_enable BOOST_PP_CAT(ENABLE_EXCEPTIONS_, __LINE__)(false)

namespace test {
  static char const* scope = "";
  bool exceptions_enabled = false;

  class scope_guard
  {
    scope_guard& operator=(scope_guard const&);
    scope_guard(scope_guard const&);

    char const* old_scope_;
    char const* scope_;
    bool dismissed_;

  public:
    scope_guard(char const* name)
        : old_scope_(scope), scope_(name), dismissed_(false)
    {
      scope = scope_;
    }

    ~scope_guard()
    {
      if (dismissed_)
        scope = old_scope_;
    }

    void dismiss() { dismissed_ = true; }

    bool dismissed() const { return dismissed_; }
  };

  class exceptions_enable
  {
    exceptions_enable& operator=(exceptions_enable const&);
    exceptions_enable(exceptions_enable const&);

    bool old_value_;
    bool released_;

  public:
    exceptions_enable(bool enable)
        : old_value_(exceptions_enabled), released_(false)
    {
      exceptions_enabled = enable;
    }

    ~exceptions_enable()
    {
      if (!released_) {
        exceptions_enabled = old_value_;
        released_ = true;
      }
    }

    void release()
    {
      if (!released_) {
        exceptions_enabled = old_value_;
        released_ = true;
      }
    }
  };

  struct exception_base
  {
    struct data_type
    {
    };
    struct strong_type
    {
      template <class T> void store(T const&) {}
      template <class T> void test(T const&) const {}
    };
    data_type init() const { return data_type(); }
    void check BOOST_PREVENT_MACRO_SUBSTITUTION() const {}
  };

  template <class T, class P1, class P2, class T2>
  inline void call_ignore_extra_parameters(
    void (T::*fn)() const, T2 const& obj, P1&, P2&)
  {
    (obj.*fn)();
  }

  template <class T, class P1, class P2, class T2>
  inline void call_ignore_extra_parameters(
    void (T::*fn)(P1&) const, T2 const& obj, P1& p1, P2&)
  {
    (obj.*fn)(p1);
  }

  template <class T, class P1, class P2, class T2>
  inline void call_ignore_extra_parameters(
    void (T::*fn)(P1&, P2&) const, T2 const& obj, P1& p1, P2& p2)
  {
    (obj.*fn)(p1, p2);
  }

  template <class T> T const& constant(T const& x) { return x; }

  template <class Test> class test_runner
  {
    Test const& test_;
    bool exception_in_check_;

    test_runner(test_runner const&);
    test_runner& operator=(test_runner const&);

  public:
    test_runner(Test const& t) : test_(t), exception_in_check_(false) {}
    void run()
    {
      DISABLE_EXCEPTIONS;
      test::check_instances check;
      test::scope = "";
      typename Test::data_type x(test_.init());
      typename Test::strong_type strong;
      strong.store(x);
      try {
        ENABLE_EXCEPTIONS;
        call_ignore_extra_parameters<Test, typename Test::data_type,
          typename Test::strong_type>(&Test::run, test_, x, strong);
      } catch (...) {
        try {
          DISABLE_EXCEPTIONS;
          call_ignore_extra_parameters<Test, typename Test::data_type const,
            typename Test::strong_type const>(
            &Test::check, test_, constant(x), constant(strong));
        } catch (...) {
          exception_in_check_ = true;
        }
        throw;
      }
    }
    void end()
    {
      if (exception_in_check_) {
        BOOST_ERROR("Unexcpected exception in test_runner check call.");
      }
    }
  };

  // Quick exception testing based on lightweight test

  namespace lightweight {
    static int iteration;
    static int count;

    struct test_exception
    {
      char const* name;
      test_exception(char const* n) : name(n) {}
    };

    struct test_failure
    {
    };

    void epoint(char const* name)
    {
      ++count;
      if (count == iteration) {
        throw test_exception(name);
      }
    }

    template <class Test>
    void exception_safety(Test const& f, char const* /*name*/)
    {
      test_runner<Test> runner(f);

      iteration = 0;
      bool success = false;
      unsigned int failure_count = 0;
      char const* error_msg = 0;
      do {
        int error_count = boost::detail::test_errors();
        ++iteration;
        count = 0;

        try {
          runner.run();
          success = true;
        } catch (test_failure) {
          error_msg = "test_failure caught.";
          break;
        } catch (test_exception e) {
          if (error_count != boost::detail::test_errors()) {
            BOOST_LIGHTWEIGHT_TEST_OSTREAM
              << "Iteration: " << iteration
              << " Error found for epoint: " << e.name << std::endl;
          }
        } catch (...) {
          error_msg = "Unexpected exception.";
          break;
        }

        if (error_count != boost::detail::test_errors()) {
          ++failure_count;
        }
      } while (!success && failure_count < 5);

      if (error_msg) {
        BOOST_ERROR(error_msg);
      }
      runner.end();
    }

    //
    // An alternative way to run exception tests.
    // See merge_exception_tests.cpp for an example.

    struct exception_looper
    {
      bool success;
      unsigned int failure_count;
      char const* error_msg;
      int error_count;

      exception_looper() : success(false), failure_count(0), error_msg(0) {}

      void start() { iteration = 0; }

      bool loop_condition() const
      {
        return !error_msg && !success && failure_count < 5;
      }

      void start_iteration()
      {
        error_count = boost::detail::test_errors();
        ++iteration;
        count = 0;
      }

      void successful_run() { success = true; }

      void test_failure_caught(test_failure const&)
      {
        error_msg = "test_failure caught.";
      }

      void test_exception_caught(test_exception const& e)
      {
        if (error_count != boost::detail::test_errors()) {
          BOOST_LIGHTWEIGHT_TEST_OSTREAM
            << "Iteration: " << iteration
            << " Error found for epoint: " << e.name << std::endl;
        }
      }

      void unexpected_exception_caught()
      {
        error_msg = "Unexpected exception.";
      }

      void end()
      {
        if (error_msg) {
          BOOST_ERROR(error_msg);
        }
      }
    };

#define EXCEPTION_LOOP(op)                                                     \
  test::lightweight::exception_looper looper;                                  \
  looper.start();                                                              \
  while (looper.loop_condition()) {                                            \
    looper.start_iteration();                                                  \
    try {                                                                      \
      op;                                                                      \
      looper.successful_run();                                                 \
    } catch (test::lightweight::test_failure e) {                              \
      looper.test_failure_caught(e);                                           \
    } catch (test::lightweight::test_exception e) {                            \
      looper.test_exception_caught(e);                                         \
    } catch (...) {                                                            \
      looper.unexpected_exception_caught();                                    \
    }                                                                          \
  }                                                                            \
  looper.end();
  }
}

#endif
