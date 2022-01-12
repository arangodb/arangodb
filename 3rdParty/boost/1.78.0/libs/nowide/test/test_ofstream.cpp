//  Copyright (c) 2015 Artyom Beilis (Tonkikh)
//  Copyright (c) 2019-2021 Alexander Grund
//
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/nowide/fstream.hpp>

#include <boost/nowide/cstdio.hpp>
#include "file_test_helpers.hpp"
#include "test.hpp"
#include <string>

namespace nw = boost::nowide;
using namespace boost::nowide::test;

template<typename T>
void test_ctor(const T& filename)
{
    // Create file if not exist
    ensure_not_exists(filename);
    {
        nw::ofstream f(filename);
        TEST(f);
    }
    TEST(file_exists(filename));
    TEST_EQ(read_file(filename), "");

    // Clear file if exists
    create_file(filename, "test");
    // Default
    {
        nw::ofstream f(filename);
        TEST(f);
    }
    TEST_EQ(read_file(filename), "");

    // At end
    {
        nw::ofstream f(filename, std::ios::ate);
        TEST(f);
    }
    TEST_EQ(read_file(filename), "");

    // Binary mode
    {
        nw::ofstream f(filename, std::ios::binary);
        TEST(f);
        TEST(f << "test\r\n");
    }
    TEST_EQ(read_file(filename, data_type::binary), "test\r\n");
}

template<typename T>
void test_open(const T& filename)
{
    // Create file if not exist
    ensure_not_exists(filename);
    {
        nw::ofstream f;
        f.open(filename);
        TEST(f);
    }
    TEST(file_exists(filename));
    TEST_EQ(read_file(filename), "");

    // Clear file if exists
    create_file(filename, "test");
    // Default
    {
        nw::ofstream f;
        f.open(filename);
        TEST(f);
    }
    TEST_EQ(read_file(filename), "");

    // At end
    {
        nw::ofstream f;
        f.open(filename, std::ios::ate);
        TEST(f);
    }
    TEST_EQ(read_file(filename), "");

    // Binary mode
    {
        nw::ofstream f;
        f.open(filename, std::ios::binary);
        TEST(f);
        TEST(f << "test\r\n");
    }
    TEST_EQ(read_file(filename, data_type::binary), "test\r\n");
}

void test_move_and_swap(const std::string& filename)
{
    const std::string filename2 = filename + ".2";
    remove_file_at_exit _(filename);
    remove_file_at_exit _2(filename2);

    // Move construct
    {
        nw::ofstream f_old(filename);
        TEST(f_old << "Hello ");

        nw::ofstream f_new(std::move(f_old));
        // old is closed
        TEST(!f_old.is_open());
        // It is unclear if the std streams can be reused after move-from
#if BOOST_NOWIDE_USE_FILEBUF_REPLACEMENT
        f_old.open(filename2);
#else
        f_old = nw::ofstream(filename2);
#endif
        TEST(f_old << "Foo");

        // new starts where the old was left of
        TEST(f_new);
        TEST(f_new << "World");
    }
    TEST_EQ(read_file(filename), "Hello World");
    TEST_EQ(read_file(filename2), "Foo");

    // Move assign
    {
        nw::ofstream f_new(filename2);
        TEST(f_new << "DiscardThis");
        {
            nw::ofstream f_old(filename);
            TEST(f_old << "Hello ");

            f_new = std::move(f_old);
            // old is closed
            TEST(!f_old.is_open());
            // It is unclear if the std streams can be reused after move-from
#if BOOST_NOWIDE_USE_FILEBUF_REPLACEMENT
            f_old.open(filename2);
#else
            f_old = nw::ofstream(filename2);
#endif
            TEST(f_old << "Foo");
        }
        // new starts where the old was left of
        TEST(f_new);
        TEST(f_new << "World");
    }
    TEST_EQ(read_file(filename), "Hello World");
    TEST_EQ(read_file(filename2), "Foo");

    // Swap
    {
        nw::ofstream f_old(filename);
        TEST(f_old << "Hello ");

        nw::ofstream f_new(filename2);
        TEST(f_new << "Foo ");

        // After swapping both are valid and where they left
        f_new.swap(f_old);
        TEST(f_old << "Bar");
        TEST(f_new << "World");

        f_new.close();
        swap(f_new, f_old);
        TEST(!f_old.is_open());
        TEST(f_new.is_open());
    }
    TEST_EQ(read_file(filename), "Hello World");
    TEST_EQ(read_file(filename2), "Foo Bar");
}

void test_main(int, char** argv, char**) // coverity [root_function]
{
    const std::string exampleFilename = std::string(argv[0]) + "-\xd7\xa9-\xd0\xbc-\xce\xbd.txt";

    test_ctor(exampleFilename.c_str());
    test_ctor(exampleFilename);
    test_open(exampleFilename.c_str());
    test_open(exampleFilename);
    test_move_and_swap(exampleFilename);
}
