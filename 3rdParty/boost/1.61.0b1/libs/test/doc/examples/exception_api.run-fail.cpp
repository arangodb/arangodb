//  (C) Copyright 2015 Boost.Test team.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE example
#include <boost/test/included/unit_test.hpp>
#include <stdexcept>
#include <fstream>

//! Computes the histogram of the words in a text file
class FileWordHistogram
{
public:
  //!@throw std::exception if the file does not exist
  FileWordHistogram(std::string filename) : is_processed(false), fileStream_(filename) {
    if(!fileStream_.is_open()) throw std::runtime_error("Cannot open the file");
  }

  //! @returns true on success, false otherwise
  bool process() {
    if(!is_processed) return true;

    // ...
    is_processed = true;
    return true;
  }

  //!@pre process has been called with status success
  //!@throw std::logic_error if preconditions not met
  std::map<std::string, std::size_t>
  result() const {
    if(!is_processed)
      throw std::runtime_error("\"process\" has not been called or was not successful");
    return histogram;
  }

private:
  bool is_processed;
  std::ifstream fileStream_;
  std::map<std::string, std::size_t> histogram;
};

BOOST_AUTO_TEST_CASE( test_throw_behaviour )
{
  // __FILE__ is accessible, no exception expected
  BOOST_REQUIRE_NO_THROW( FileWordHistogram(__FILE__) );

  // ".. __FILE__" does not exist, API says std::exception, and implementation
  // raises std::runtime_error child of std::exception
  BOOST_CHECK_THROW( FileWordHistogram(".." __FILE__), std::exception );

  {
    FileWordHistogram instance(__FILE__);

    // api says "std::logic_error", implementation is wrong.
    // std::runtime_error not a child of std::logic_error, not intercepted
    // here.
    BOOST_CHECK_THROW(instance.result(), std::logic_error);
  }
}
//]
