//  Copyright (c) 2015 Artyom Beilis (Tonkikh)
//  Copyright (c) 2019-2021 Alexander Grund
//
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/nowide/fstream.hpp>

#include "file_test_helpers.hpp"
#include "test.hpp"
#include <string>

namespace nw = boost::nowide;
using namespace boost::nowide::test;

template<typename T>
void test_ctor(const T& filename)
{
    // Fail on non-existing file
    ensure_not_exists(filename);
    {
        nw::ifstream f(filename);
        TEST(!f);
    }
    TEST(!file_exists(filename));

    create_file(filename, "test");

    // Default
    {
        nw::ifstream f(filename);
        TEST(f);
        std::string tmp;
        TEST(f >> tmp);
        TEST(tmp == "test");
    }
    TEST(read_file(filename) == "test");

    // At end
    {
        nw::ifstream f(filename, std::ios::ate);
        TEST(f);
        std::string tmp;
        TEST(!(f >> tmp));
        TEST(f.eof());
        f.clear();
        f.seekg(0, std::ios::beg);
        TEST(f >> tmp);
        TEST(tmp == "test");
    }
    TEST(read_file(filename) == "test");

    create_file(filename, "test\r\n", data_type::binary);
    // Binary mode
    {
        nw::ifstream f(filename, std::ios::binary);
        TEST(f);
        std::string tmp(6, '\0');
        TEST(f.read(&tmp[0], 6));
        TEST(tmp == "test\r\n");
    }
}

template<typename T>
void test_open(const T& filename)
{
    // Fail on non-existing file
    ensure_not_exists(filename);
    {
        nw::ifstream f;
        f.open(filename);
        TEST(!f);
    }
    TEST(!file_exists(filename));

    create_file(filename, "test");

    // Default
    {
        nw::ifstream f;
        f.open(filename);
        TEST(f);
        std::string tmp;
        TEST(f >> tmp);
        TEST(tmp == "test");
    }
    TEST(read_file(filename) == "test");

    // At end
    {
        nw::ifstream f;
        f.open(filename, std::ios::ate);
        TEST(f);
        std::string tmp;
        TEST(!(f >> tmp));
        TEST(f.eof());
        f.clear();
        f.seekg(0, std::ios::beg);
        TEST(f >> tmp);
        TEST(tmp == "test");
    }
    TEST(read_file(filename) == "test");

    create_file(filename, "test\r\n", data_type::binary);
    // Binary mode
    {
        nw::ifstream f;
        f.open(filename, std::ios::binary);
        TEST(f);
        std::string tmp(6, '\0');
        TEST(f.read(&tmp[0], 6));
        TEST(tmp == "test\r\n");
    }
}

void test_move_and_swap(const std::string& filename)
{
    const std::string filename2 = filename + ".2";
    create_file(filename, "Hello\nWorld");
    create_file(filename2, "Foo\nBar");
    remove_file_at_exit _(filename);
    remove_file_at_exit _2(filename);

    // Move construct
    {
        nw::ifstream f_old(filename);
        std::string s;
        TEST(f_old >> s && s == "Hello");

        nw::ifstream f_new(std::move(f_old));
        // old is closed
        TEST(!f_old.is_open());
        // It is unclear if the std streams can be reused after move-from
#if BOOST_NOWIDE_USE_FILEBUF_REPLACEMENT
        f_old.open(filename2);
#else
        f_old = nw::ifstream(filename2);
#endif
        TEST(f_old);
        TEST(f_old >> s);
        TEST(s == "Foo");
        TEST(f_old >> s && s == "Bar");

        // new starts where the old was left of
        TEST(f_new);
        TEST(f_new >> s);
        TEST(s == "World");
    }
    // Move assign
    {
        nw::ifstream f_new(filename2);
        std::string s;
        TEST(f_new >> s && s == "Foo");
        {
            nw::ifstream f_old(filename);
            TEST(f_old >> s && s == "Hello");

            f_new = std::move(f_old);
            // old is closed
            TEST(!f_old.is_open());
            // It is unclear if the std streams can be reused after move-from
#if BOOST_NOWIDE_USE_FILEBUF_REPLACEMENT
            f_old.open(filename2);
#else
            f_old = nw::ifstream(filename2);
#endif
            TEST(f_old);
            TEST(f_old >> s);
            TEST(s == "Foo");
            TEST(f_old >> s && s == "Bar");
        }
        // new starts where the old was left of
        TEST(f_new);
        TEST(f_new >> s);
        TEST(s == "World");
    }
    // Swap
    {
        nw::ifstream f_old(filename);
        std::string s;
        TEST(f_old >> s && s == "Hello");

        nw::ifstream f_new(filename2);
        TEST(f_new >> s && s == "Foo");

        // After swapping both are valid and where they left
        f_new.swap(f_old);
        TEST(f_old >> s);
        TEST(s == "Bar");

        TEST(f_new >> s);
        TEST(s == "World");

        f_new.close();
        swap(f_new, f_old);
        TEST(!f_old.is_open());
        TEST(f_new.is_open());
    }
}

// coverity [root_function]
void test_main(int, char** argv, char**)
{
    const std::string exampleFilename = std::string(argv[0]) + "-\xd7\xa9-\xd0\xbc-\xce\xbd.txt";

    test_ctor(exampleFilename.c_str());
    test_ctor(exampleFilename);
    test_open(exampleFilename.c_str());
    test_open(exampleFilename);
    test_move_and_swap(exampleFilename);
}
