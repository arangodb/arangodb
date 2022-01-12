//  Copyright (c) 2019-2021 Alexander Grund
//
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NOWIDE_FILE_TEST_HELPERS_HPP_INCLUDED
#define BOOST_NOWIDE_FILE_TEST_HELPERS_HPP_INCLUDED

#include <string>

namespace boost {
namespace nowide {
    namespace test {
        enum class data_type
        {
            text,
            binary
        };

        void create_empty_file(const std::string& filepath);

        void create_file(const std::string& filepath, const std::string& contents, data_type type = data_type::text);

        bool file_exists(const std::string& filepath);

        std::string read_file(const std::string& filepath, data_type type = data_type::text);

        void ensure_not_exists(const char* filepath);
        inline void ensure_not_exists(const std::string& filepath)
        {
            ensure_not_exists(filepath.c_str());
        }

        std::string create_random_data(size_t num_chars, data_type type);

        struct remove_file_at_exit
        {
            const std::string filepath;
            explicit remove_file_at_exit(std::string filepath) : filepath(std::move(filepath))
            {}
            ~remove_file_at_exit() noexcept(false)
            {
                ensure_not_exists(filepath);
            }
        };

    } // namespace test
} // namespace nowide
} // namespace boost

#endif // BOOST_NOWIDE_FILE_TEST_HELPERS_HPP_INCLUDED