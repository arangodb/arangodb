//  Copyright (c) 2019 Raffi Enficiaud
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE runtime_configuration4

#include <boost/test/included/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

#include <iostream>
#include <functional>
#include <sstream>
#include <fstream>

// this dataset loads a file that contains a list of strings
// this list is used to create a dataset test case.
class file_dataset
{
private:
    std::string m_filename;
    std::size_t m_line_start;
    std::size_t m_line_end;

public:
    enum { arity = 2 };

public:
    file_dataset(std::size_t line_start = 0, std::size_t line_end = std::size_t(-1))
    : m_line_start(line_start)
    , m_line_end(line_end)
    {
      int argc = boost::unit_test::framework::master_test_suite().argc;
      char** argv = boost::unit_test::framework::master_test_suite().argv;

      if(argc != 3)
        throw std::logic_error("Incorrect number of arguments");
      if(std::string(argv[1]) != "--test-file")
        throw std::logic_error("First argument != '--test-file'");
      if(!(m_line_start < std::size_t(-1)))
        throw std::logic_error("Incorrect line start/end");

      m_filename = argv[2];

      std::ifstream file(m_filename);
      if(!file.is_open())
        throw std::logic_error("Cannot open the file '" + m_filename + "'");
      std::size_t nb_lines = std::count_if(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>(),
        [](char c){ return c == '\n';});

      m_line_end = (std::min)(nb_lines, m_line_end);
      if(!(m_line_start <= m_line_end))
        throw std::logic_error("Incorrect line start/end");
    }

    struct iterator {
        iterator(std::string const& filename, std::size_t line_start)
        : file(filename, std::ios::binary) {
          if(!file.is_open())
            throw std::runtime_error("Cannot open the file");
          for(std::size_t i = 0; i < line_start; i++) {
            getline(file, m_current_line);
          }
        }

        auto operator*() const -> std::tuple<float, float> {
          float a, b;
          std::istringstream istr(m_current_line);
          istr >> a >> b;
          return std::tuple<float, float>(a, b);
        }

        void operator++() {
          getline(file, m_current_line);
        }
    private:
        std::ifstream file;
        std::string m_current_line;
    };

    // size of the DS
    boost::unit_test::data::size_t size() const {
      return m_line_end - m_line_start;
    }

    // iterator over the lines of the file
    iterator begin() const   {
      return iterator(m_filename, m_line_start);
    }
};

namespace boost { namespace unit_test { namespace data {

namespace monomorphic {
  template <>
  struct is_dataset<file_dataset> : boost::mpl::true_ {};
}
}}}

BOOST_DATA_TEST_CASE(command_line_test_file,
    boost::unit_test::data::make_delayed<file_dataset>( 3, 10 ),
    input, expected) {
    BOOST_TEST(input <= expected);
}
//]
