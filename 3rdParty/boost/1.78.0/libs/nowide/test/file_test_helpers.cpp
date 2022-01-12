//  Copyright (c) 2019-2021 Alexander Grund
//
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_NOWIDE_TEST_NO_MAIN
#include "file_test_helpers.hpp"

#include <boost/nowide/cstdio.hpp>
#include "test.hpp"
#include <algorithm>
#include <limits>
#include <numeric>
#include <random>

namespace boost {
namespace nowide {
    namespace test {

        void create_file(const std::string& filepath, const std::string& contents, data_type type /*= data_type::text*/)
        {
            auto* f = fopen(filepath.c_str(), (type == data_type::binary) ? "wb" : "w");
            TEST(f);
            TEST(std::fwrite(contents.data(), 1, contents.size(), f) == contents.size());
            std::fclose(f);
        }

        bool file_exists(const std::string& filepath)
        {
            FILE* f = fopen(filepath.c_str(), "r");
            if(f)
            {
                std::fclose(f);
                return true;
            } else
                return false;
        }

        std::string read_file(const std::string& filepath, data_type type /*= data_type::text*/)
        {
            FILE* f = fopen(filepath.c_str(), (type == data_type::binary) ? "rb" : "r");
            TEST(f);
            std::fseek(f, 0, SEEK_END);
            const auto file_size_tmp = ftell(f);
            TEST(file_size_tmp >= 0);
            const auto file_size = static_cast<size_t>(file_size_tmp);
            std::fseek(f, 0, SEEK_SET);
            std::string content(file_size, '\0');
            const auto read_bytes = std::fread(&content[0], 1, file_size, f);
            if(read_bytes != file_size)
            {
                TEST(std::feof(f));
                content.resize(read_bytes);
            }
            std::fclose(f);
            return content;
        }

        void ensure_not_exists(const char* filepath)
        {
            if(file_exists(filepath))
            {
                remove(filepath);
                TEST(!file_exists(filepath));
            }
        }

        static std::string get_ascii_chars()
        {
            const auto first_char = char(0x20);
            const auto last_char = char(0x7E);
            std::string result(last_char - first_char + 2, '\0');
            result[0] = '\n'; // Include newline
            std::iota(result.begin() + 1, result.end(), first_char);
            return result;
        }

        std::string create_random_data(size_t num_chars, data_type type)
        {
            static std::minstd_rand rng(std::random_device{}());
            std::string result(num_chars, '\0');
            if(type == data_type::binary)
            {
                std::uniform_int_distribution<int> distr(std::numeric_limits<char>::min(),
                                                         std::numeric_limits<char>::max());
                std::generate(result.begin(), result.end(), [&]() { return static_cast<char>(distr(rng)); });
            } else
            {
                static const std::string text_data = get_ascii_chars();
                std::uniform_int_distribution<std::size_t> distr(0, text_data.size() - 1u);
                std::generate(result.begin(), result.end(), [&]() { return text_data[distr(rng)]; });
            }
            return result;
        }

    } // namespace test
} // namespace nowide
} // namespace boost