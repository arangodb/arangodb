//  Copyright (c) 2015 Artyom Beilis (Tonkikh)
//  Copyright (c) 2019-2021 Alexander Grund
//
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/nowide/fstream.hpp>

#include <boost/nowide/convert.hpp>
#include <boost/nowide/cstdio.hpp>
#include "file_test_helpers.hpp"
#include "test.hpp"
#include <fstream>
#include <iostream>
#include <string>

namespace nw = boost::nowide;
using namespace boost::nowide::test;

template<class T>
void test_ctor(const T& filename)
{
    remove_file_at_exit _(filename);

    // Fail on non-existing file
    {
        ensure_not_exists(filename);
        nw::fstream f(filename);
        TEST(!f);
    }
    TEST(!file_exists(filename));

    // Create empty file
    {
        ensure_not_exists(filename);
        nw::fstream f(filename, std::ios::out);
        TEST(f);
    }
    TEST(read_file(filename).empty());

    // Read+write existing file
    create_file(filename, "Hello");
    {
        nw::fstream f(filename);
        std::string tmp;
        TEST(f >> tmp);
        TEST(f.eof());
        TEST(tmp == "Hello");
        f.clear();
        TEST(f << "World");
    }
    TEST(read_file(filename) == "HelloWorld");
    create_file(filename, "Hello");
    {
        nw::fstream f(filename, std::ios::out | std::ios::in);
        std::string tmp;
        TEST(f >> tmp);
        TEST(f.eof());
        TEST(tmp == "Hello");
        f.clear();
        TEST(f << "World");
    }
    TEST(read_file(filename) == "HelloWorld");

    // Readonly existing file
    create_file(filename, "Hello");
    {
        nw::fstream f(filename, std::ios::in);
        std::string tmp;
        TEST(f >> tmp);
        TEST(tmp == "Hello");
        f.clear();
        TEST(f);
        TEST(!(f << "World"));
    }
    TEST(read_file(filename) == "Hello");

    // Write existing file
    create_file(filename, "Hello");
    {
        nw::fstream f(filename, std::ios::out);
        std::string tmp;
        TEST(f);
        TEST(!(f >> tmp));
        f.clear();
        TEST(f << "World");
    }
    TEST(read_file(filename) == "World");
    // Write existing file with explicit trunc
    create_file(filename, "Hello");
    {
        nw::fstream f(filename, std::ios::out | std::ios::trunc);
        std::string tmp;
        TEST(f);
        TEST(!(f >> tmp));
        f.clear();
        TEST(f << "World");
    }
    TEST(read_file(filename) == "World");

    // append existing file
    create_file(filename, "Hello");
    {
        nw::fstream f(filename, std::ios::app);
        TEST(f << "World");
    }
    TEST(read_file(filename) == "HelloWorld");
    create_file(filename, "Hello");
    {
        nw::fstream f(filename, std::ios::out | std::ios::app);
        TEST(f << "World");
    }
    TEST(read_file(filename) == "HelloWorld");

    // read+write+truncate existing file
    create_file(filename, "Hello");
    {
        nw::fstream f(filename, std::ios::out | std::ios::in | std::ios::trunc);
        std::string tmp;
        TEST(f);
        TEST(!(f >> tmp));
        f.clear();
        TEST(f << "World");
        f.seekg(0);
        TEST(f >> tmp);
        TEST(tmp == "World");
    }
    TEST(read_file(filename) == "World");

    // read+write+append existing file
    create_file(filename, "Hello");
    {
        nw::fstream f(filename, std::ios::out | std::ios::in | std::ios::app);
        TEST(f);
        TEST(f.seekg(0)); // It is not defined where the read position is after opening
        TEST(f.tellg() == std::streampos(0));
        std::string tmp;
        TEST(f >> tmp);
        TEST(tmp == "Hello");
        f.seekg(0);
        TEST(f << "World");
    }
    TEST(read_file(filename) == "HelloWorld");
    create_file(filename, "Hello");
    {
        nw::fstream f(filename, std::ios::in | std::ios::app);
        std::string tmp;
        TEST(f.seekg(0)); // It is not defined where the read position is after opening
        TEST(f.tellg() == std::streampos(0));
        TEST(f >> tmp);
        TEST(tmp == "Hello");
        f.seekg(0);
        TEST(f << "World");
    }
    TEST(read_file(filename) == "HelloWorld");

    // Write at end
    create_file(filename, "Hello");
    {
        nw::fstream f(filename, std::ios::out | std::ios::in | std::ios::ate);
        std::string tmp;
        TEST(f);
        TEST(!(f >> tmp));
        f.clear();
        TEST(f << "World");
        f.seekg(0);
        TEST(f >> tmp);
        TEST(tmp == "HelloWorld");
    }
    TEST(read_file(filename) == "HelloWorld");

    // Trunc & binary
    create_file(filename, "Hello");
    {
        nw::fstream f(filename, std::ios::in | std::ios::out | std::ios::trunc | std::ios::binary);
        TEST(f);
        TEST(f << "test\r\n");
        std::string tmp(6, '\0');
        TEST(f.seekg(0));
        TEST(f.read(&tmp[0], 6));
        TEST(tmp == "test\r\n");
    }
    TEST(read_file(filename, data_type::binary) == "test\r\n");

    // Invalid modes
    const std::initializer_list<std::ios::openmode> invalid_modes{
      // clang-format off
                                     std::ios::trunc,
                                     std::ios::trunc | std::ios::app,
      std::ios::out |                std::ios::trunc | std::ios::app,
                      std::ios::in | std::ios::trunc,
                      std::ios::in | std::ios::trunc | std::ios::app,
      std::ios::out | std::ios::in | std::ios::trunc | std::ios::app
      // clang-format on
    };
    for(const auto mode : invalid_modes)
    {
        create_file(filename, "Hello");
        {
            nw::fstream f(filename, mode);
            TEST(!f);
        }
        TEST(read_file(filename) == "Hello");
    }
}

template<typename T>
void test_open(const T& filename)
{
    remove_file_at_exit _(filename);

    // Fail on non-existing file
    {
        ensure_not_exists(filename);
        nw::fstream f;
        f.open(filename);
        TEST(!f);
    }
    TEST(!file_exists(filename));

    // Create empty file
    {
        ensure_not_exists(filename);
        nw::fstream f;
        f.open(filename, std::ios::out);
        TEST(f);
    }
    TEST(read_file(filename).empty());

    // Read+write existing file
    create_file(filename, "Hello");
    {
        nw::fstream f;
        f.open(filename);
        std::string tmp;
        TEST(f >> tmp);
        TEST(f.eof());
        TEST(tmp == "Hello");
        f.clear();
        TEST(f << "World");
    }
    TEST(read_file(filename) == "HelloWorld");

    // Readonly existing file
    create_file(filename, "Hello");
    {
        nw::fstream f;
        f.open(filename, std::ios::in);
        std::string tmp;
        TEST(f >> tmp);
        TEST(tmp == "Hello");
        f.clear();
        TEST(f);
        TEST(!(f << "World"));
    }
    TEST(read_file(filename) == "Hello");

    // remaining mode cases skipped as they are already tested by the ctor tests
}

template<typename T>
bool is_open(T& stream)
{
    // There are const and non const versions of is_open, so test both
    TEST(stream.is_open() == const_cast<const T&>(stream).is_open());
    return stream.is_open();
}

template<typename T>
void do_test_is_open(const std::string& filename)
{
    T f;
    TEST(!is_open(f));
    f.open(filename);
    TEST(f);
    TEST(is_open(f));
    f.close();
    TEST(f);
    TEST(!is_open(f));
}

/// Test is_open for all 3 fstream classes
void test_is_open(const std::string& filename)
{
    // Note the order: Output before input so file exists
    do_test_is_open<nw::ofstream>(filename);
    remove_file_at_exit _(filename);
    do_test_is_open<nw::ifstream>(filename);
    do_test_is_open<nw::fstream>(filename);
}

void test_move_and_swap(const std::string& filename)
{
    const std::string filename2 = filename + ".2";
    create_file(filename2, "Foo Bar");
    remove_file_at_exit _(filename);
    remove_file_at_exit _2(filename);

    // Move construct
    {
        nw::fstream f_old(filename, std::ios::out);
        TEST(f_old << "Hello ");

        nw::fstream f_new(std::move(f_old));
        // old is closed
        TEST(!f_old.is_open());
        // It is unclear if the std streams can be reused after move-from
#if BOOST_NOWIDE_USE_FILEBUF_REPLACEMENT
        f_old.open(filename2, std::ios::in);
#else
        f_old = nw::fstream(filename2, std::ios::in);
#endif
        std::string s;
        TEST(f_old);
        TEST(f_old >> s);
        TEST(s == "Foo");

        // new starts where the old was left of
        TEST(f_new);
        TEST(f_new << "World");
    }
    TEST(read_file(filename) == "Hello World");
    TEST(read_file(filename2) == "Foo Bar");

    // Move assign
    {
        nw::fstream f_new(filename2);
        TEST(f_new << "ReadThis");
        {
            nw::fstream f_old(filename, std::ios::out);
            TEST(f_old << "Hello ");

            f_new = std::move(f_old);
            // old is closed
            TEST(!f_old.is_open());
            // It is unclear if the std streams can be reused after move-from
#if BOOST_NOWIDE_USE_FILEBUF_REPLACEMENT
            f_old.open(filename2, std::ios::in);
#else
            f_old = nw::fstream(filename2, std::ios::in);
#endif
            std::string s;
            TEST(f_old >> s);
            TEST(s == "ReadThis");
        }
        // new starts where the old was left of
        TEST(f_new);
        TEST(f_new << "World");
    }
    TEST(read_file(filename) == "Hello World");
    TEST(read_file(filename2) == "ReadThis");

    create_file(filename2, "Foo Bar");
    // Swap
    {
        nw::fstream f_old(filename, std::ios::out);
        TEST(f_old << "Hello ");

        nw::fstream f_new(filename2, std::ios::in);
        std::string s;
        TEST(f_new >> s);
        TEST(s == "Foo");

        // After swapping both are valid and where they left
        f_new.swap(f_old);
        TEST(f_old >> s);
        TEST(s == "Bar");
        TEST(f_new << "World");

        f_new.close();
        swap(f_new, f_old);
        TEST(!f_old.is_open());
        TEST(f_new.is_open());
    }
    TEST(read_file(filename) == "Hello World");
    TEST(read_file(filename2) == "Foo Bar");
}

void test_flush(const std::string& filepath)
{
    remove_file_at_exit _(filepath);
    nw::fstream fo(filepath, std::ios_base::out | std::ios::trunc);
    TEST(fo);
    std::string curValue;
    for(int repeat = 0; repeat < 2; repeat++)
    {
        for(size_t len = 1; len <= 1024; len *= 2)
        {
            char c = static_cast<char>(len % 13 + repeat + 'a'); // semi-random char
            std::string input(len, c);
            fo << input;
            curValue += input;
            TEST(fo.flush());
            std::string s;
            // Note: Flush on read area is implementation defined, so check whole file instead
            nw::fstream fi(filepath, std::ios_base::in);
            TEST(fi >> s);
            // coverity[tainted_data]
            TEST(s == curValue);
        }
    }
}

// coverity [root_function]
void test_main(int, char** argv, char**)
{
    const std::string exampleFilename = std::string(argv[0]) + "-\xd7\xa9-\xd0\xbc-\xce\xbd.txt";

    std::cout << "Ctor" << std::endl;
    test_ctor<const char*>(exampleFilename.c_str());
    test_ctor<std::string>(exampleFilename);

    std::cout << "Open" << std::endl;
    test_open<const char*>(exampleFilename.c_str());
    test_open<std::string>(exampleFilename);

    std::cout << "IsOpen" << std::endl;
    test_is_open(exampleFilename);

    std::cout << "Move and swap" << std::endl;
    test_move_and_swap(exampleFilename);

    std::cout << "Flush" << std::endl;
    test_flush(exampleFilename);
}
