//  (C) Copyright Raffi Enficiaud 2017.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//  tests Unit Test Framework logging facilities against
//  pattern file
// ***************************************************************************


#ifndef BOOST_TEST_TESTS_LOGGER_FOR_TESTS_HPP__
#define BOOST_TEST_TESTS_LOGGER_FOR_TESTS_HPP__

#include <boost/test/tools/output_test_stream.hpp>
#include <boost/test/utils/algorithm.hpp>

using namespace boost::unit_test;
using boost::test_tools::output_test_stream;

class output_test_stream_for_loggers : public output_test_stream {

    std::string const source_filename;
    std::string const basename;

public:
    explicit output_test_stream_for_loggers(
        boost::unit_test::const_string    pattern_file_name = boost::unit_test::const_string(),
        bool                              match_or_save     = true,
        bool                              text_or_binary    = true,
        const std::string&                source_filename_  = __FILE__)
    : output_test_stream(pattern_file_name, match_or_save, text_or_binary)
    , source_filename(source_filename_)
    , basename(get_basename(source_filename_))
    {}

    static std::string normalize_path(const std::string &str) {
        const std::string to_look_for[] = {"\\"};
        const std::string to_replace[]  = {"/"};
        return utils::replace_all_occurrences_of(
                    str,
                    to_look_for, to_look_for + sizeof(to_look_for)/sizeof(to_look_for[0]),
                    to_replace, to_replace + sizeof(to_replace)/sizeof(to_replace[0])
              );
    }

    static std::string get_basename(const std::string &source_filename) {
        std::string basename = normalize_path(source_filename);
        std::string::size_type basename_pos = basename.rfind('/');
        if(basename_pos != std::string::npos) {
             basename = basename.substr(basename_pos+1);
        }
        return basename;
    }

    virtual std::string get_stream_string_representation() const {
        std::string current_string = output_test_stream::get_stream_string_representation();

        std::string pathname_fixes;
        {
            const std::string to_look_for[] = { source_filename, normalize_path(source_filename)};
            const std::string to_replace[]  = {"xxx/" + basename, "xxx/" + basename };
            pathname_fixes = utils::replace_all_occurrences_of(
                current_string,
                to_look_for, to_look_for + sizeof(to_look_for)/sizeof(to_look_for[0]),
                to_replace, to_replace + sizeof(to_replace)/sizeof(to_replace[0])
            );
        }

        std::string other_vars_fixes;
        {
            std::ostringstream s_version;
            s_version << BOOST_VERSION/100000 << "." << BOOST_VERSION/100 % 1000 << "." << BOOST_VERSION % 100;

            const std::string to_look_for[] = {"time=\"*\"",
                                               basename + "(*):",
                                               "unknown location(*):",
                                               "; testing time: *us\n", // removing this is far more easier than adding a testing time
                                               "; testing time: *ms\n",
                                               "<TestingTime>*</TestingTime>",
                                               "condition 2>3 is not satisfied\n",
                                               "condition 2>3 is not satisfied]",
                                               BOOST_PLATFORM,
                                               BOOST_STDLIB,
                                               BOOST_COMPILER,
                                               s_version.str()
                                               };

            const std::string to_replace[]  = {"time=\"0.1234\"",
                                               basename + ":*:" ,
                                               "unknown location:*:",
                                               "\n",
                                               "\n",
                                               "<TestingTime>ZZZ</TestingTime>",
                                               "condition 2>3 is not satisfied [2 <= 3]\n",
                                               "condition 2>3 is not satisfied [2 <= 3]]",
                                               "BOOST_SOME_PLATFORM",
                                               "BOOST_SOME_STDLIB",
                                               "BOOST_SOME_COMPILER",
                                               "BOOST_1.XX.Y_SOME_VERSION",
                                               };

            other_vars_fixes = utils::replace_all_occurrences_with_wildcards(
                pathname_fixes,
                to_look_for, to_look_for + sizeof(to_look_for)/sizeof(to_look_for[0]),
                to_replace, to_replace + sizeof(to_replace)/sizeof(to_replace[0])
            );
        }

        return other_vars_fixes;
    }

};


// helper for tests
struct log_setup_teardown {
  log_setup_teardown(
    output_test_stream& output,
    output_format log_format,
    log_level ll,
    log_level level_to_restore = invalid_log_level,
    output_format additional_log_format = OF_INVALID)
  : m_previous_ll(level_to_restore)
  {
    boost::unit_test::unit_test_log.set_format(log_format);
    if(additional_log_format != OF_INVALID) {
      boost::unit_test::unit_test_log.add_format(additional_log_format);
    }
    boost::unit_test::unit_test_log.set_stream(output);
    if(level_to_restore == invalid_log_level) {
      m_previous_ll = boost::unit_test::unit_test_log.set_threshold_level(ll);
    }
  }

  ~log_setup_teardown() {
    boost::unit_test::unit_test_log.set_format(OF_CLF);
    boost::unit_test::unit_test_log.set_stream(std::cout);
    boost::unit_test::unit_test_log.set_threshold_level( m_previous_ll );
    boost::unit_test::unit_test_log.configure(); // forces reconfiguration
  }

  log_level m_previous_ll;
};

#endif /* BOOST_TEST_TESTS_LOGGER_FOR_TESTS_HPP__ */
