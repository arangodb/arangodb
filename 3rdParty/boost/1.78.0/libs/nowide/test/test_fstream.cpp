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
#include <locale>
#include <string>

namespace nw = boost::nowide;
using namespace boost::nowide::test;

class dummyCvtConverting : public std::codecvt<char, char, std::mbstate_t>
{
protected:
    bool do_always_noconv() const noexcept override
    {
        return false;
    }
};

class dummyCvtNonConverting : public std::codecvt<char, char, std::mbstate_t>
{
protected:
    bool do_always_noconv() const noexcept override
    {
        return true;
    }
};

std::string get_narrow_name(const std::string& name)
{
    return name;
}
std::string get_narrow_name(const std::wstring& name) // LCOV_EXCL_LINE
{
    return boost::nowide::narrow(name);
}

template<class T>
void test_ctor(const T& filename)
{
    const std::string narrow_filename = get_narrow_name(filename);
    remove_file_at_exit _(narrow_filename);

    // Fail on non-existing file
    {
        ensure_not_exists(narrow_filename);
        nw::fstream f(filename);
        TEST(!f);
    }
    TEST(!file_exists(narrow_filename));

    // Create empty file
    {
        ensure_not_exists(narrow_filename);
        nw::fstream f(filename, std::ios::out);
        TEST(f);
    }
    TEST(read_file(narrow_filename).empty());

    // Read+write existing file
    create_file(narrow_filename, "Hello");
    {
        nw::fstream f(filename);
        TEST(f);
        std::string tmp;
        TEST(f >> tmp);
        TEST(f.eof());
        TEST_EQ(tmp, "Hello");
        f.clear();
        TEST(f << "World");
    }
    TEST_EQ(read_file(narrow_filename), "HelloWorld");
    create_file(narrow_filename, "Hello");
    {
        nw::fstream f(filename, std::ios::out | std::ios::in);
        TEST(f);
        std::string tmp;
        TEST(f >> tmp);
        TEST(f.eof());
        TEST_EQ(tmp, "Hello");
        f.clear();
        TEST(f << "World");
    }
    TEST_EQ(read_file(narrow_filename), "HelloWorld");

    // Readonly existing file
    create_file(narrow_filename, "Hello");
    {
        nw::fstream f(filename, std::ios::in);
        TEST(f);
        std::string tmp;
        TEST(f >> tmp);
        TEST_EQ(tmp, "Hello");
        f.clear();
        TEST(f);
        TEST(!(f << "World"));
    }
    TEST_EQ(read_file(narrow_filename), "Hello");

    // Write existing file
    create_file(narrow_filename, "Hello");
    {
        nw::fstream f(filename, std::ios::out);
        TEST(f);
        std::string tmp;
        TEST(!(f >> tmp));
        f.clear();
        TEST(f << "World");
    }
    TEST_EQ(read_file(narrow_filename), "World");
    // Write existing file with explicit trunc
    create_file(narrow_filename, "Hello");
    {
        nw::fstream f(filename, std::ios::out | std::ios::trunc);
        TEST(f);
        std::string tmp;
        TEST(!(f >> tmp));
        f.clear();
        TEST(f << "World");
    }
    TEST_EQ(read_file(narrow_filename), "World");

    // append existing file
    create_file(narrow_filename, "Hello");
    {
        nw::fstream f(filename, std::ios::app);
        TEST(f);
        TEST(f << "World");
    }
    TEST_EQ(read_file(narrow_filename), "HelloWorld");
    create_file(narrow_filename, "Hello");
    {
        nw::fstream f(filename, std::ios::out | std::ios::app);
        TEST(f);
        TEST(f << "World");
    }
    TEST_EQ(read_file(narrow_filename), "HelloWorld");

    // read+write+truncate existing file
    create_file(narrow_filename, "Hello");
    {
        nw::fstream f(filename, std::ios::out | std::ios::in | std::ios::trunc);
        TEST(f);
        std::string tmp;
        TEST(!(f >> tmp));
        f.clear();
        TEST(f << "World");
        f.seekg(0);
        TEST(f >> tmp);
        TEST_EQ(tmp, "World");
    }
    TEST_EQ(read_file(narrow_filename), "World");

    // read+write+append existing file
    create_file(narrow_filename, "Hello");
    {
        nw::fstream f(filename, std::ios::out | std::ios::in | std::ios::app);
        TEST(f);
        TEST(f.seekg(0)); // It is not defined where the read position is after opening
        TEST_EQ(f.tellg(), std::streampos(0));
        std::string tmp;
        TEST(f >> tmp);
        TEST_EQ(tmp, "Hello");
        f.seekg(0);
        TEST(f << "World");
    }
    TEST_EQ(read_file(narrow_filename), "HelloWorld");
    create_file(narrow_filename, "Hello");
    {
        nw::fstream f(filename, std::ios::in | std::ios::app);
        TEST(f);
        std::string tmp;
        TEST(f.seekg(0)); // It is not defined where the read position is after opening
        TEST_EQ(f.tellg(), std::streampos(0));
        TEST(f >> tmp);
        TEST_EQ(tmp, "Hello");
        f.seekg(0);
        TEST(f << "World");
    }
    TEST_EQ(read_file(narrow_filename), "HelloWorld");

    // Write at end
    create_file(narrow_filename, "Hello");
    {
        nw::fstream f(filename, std::ios::out | std::ios::in | std::ios::ate);
        TEST(f);
        std::string tmp;
        TEST(!(f >> tmp));
        f.clear();
        TEST(f << "World");
        f.seekg(0);
        TEST(f >> tmp);
        TEST_EQ(tmp, "HelloWorld");
    }
    TEST_EQ(read_file(narrow_filename), "HelloWorld");

    // binary append existing file
    create_file(narrow_filename, "Hello");
    {
        nw::fstream f(filename, std::ios::binary | std::ios::out | std::ios::app);
        TEST(f);
        TEST(f << "World");
        TEST(f.seekp(0));
        TEST(f.seekg(0));
        TEST(f << "World\n");
    }
    TEST_EQ(read_file(narrow_filename, data_type::binary), "HelloWorldWorld\n");
    create_file(narrow_filename, "Hello");
    {
        nw::fstream f(filename, std::ios::binary | std::ios::app);
        TEST(f);
        TEST(f << "World");
        TEST(f.seekp(0));
        TEST(f.seekg(0));
        TEST(f << "World\n");
    }
    TEST_EQ(read_file(narrow_filename, data_type::binary), "HelloWorldWorld\n");

    // binary out & trunc
    create_file(narrow_filename, "Hello");
    {
        nw::fstream f(filename, std::ios::binary | std::ios::out | std::ios::trunc);
        TEST(f);
        TEST(f << "Hello\n");
        TEST(f << "World");
    }
    TEST_EQ(read_file(narrow_filename, data_type::binary), "Hello\nWorld");

    // Binary in&out
    create_file(narrow_filename, "Hello");
    {
        nw::fstream f(filename, std::ios::binary | std::ios::out | std::ios::in);
        TEST(f);
        std::string tmp;
        TEST(f >> tmp);
        TEST(f.eof());
        TEST_EQ(tmp, "Hello");
        f.clear();
        TEST(f << "World\n");
    }
    TEST_EQ(read_file(narrow_filename, data_type::binary), "HelloWorld\n");

    // Trunc & binary
    create_file(narrow_filename, "Hello");
    {
        nw::fstream f(filename, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
        TEST(f);
        TEST(f << "test\n");
        std::string tmp(5, '\0');
        TEST(f.seekg(0));
        TEST(f.read(&tmp[0], 5));
        TEST_EQ(tmp, "test\n");
    }
    TEST_EQ(read_file(narrow_filename, data_type::binary), "test\n");

    // Binary in&out append
    create_file(narrow_filename, "Hello");
    {
        nw::fstream f(filename, std::ios::binary | std::ios::in | std::ios::out | std::ios::app);
        TEST(f);
        TEST(f.seekg(0)); // It is not defined where the read position is after opening
        std::string tmp;
        TEST(f >> tmp);
        TEST(f.eof());
        TEST_EQ(tmp, "Hello");
        f.clear();
        f.seekg(0);
        f.seekp(0);
        TEST(f << "World\n");
    }
    TEST_EQ(read_file(narrow_filename, data_type::binary), "HelloWorld\n");
    create_file(narrow_filename, "Hello");
    {
        nw::fstream f(filename, std::ios::binary | std::ios::in | std::ios::app);
        TEST(f);
        TEST(f.seekg(0)); // It is not defined where the read position is after opening
        std::string tmp;
        TEST(f >> tmp);
        TEST(f.eof());
        TEST_EQ(tmp, "Hello");
        f.clear();
        f.seekg(0);
        f.seekp(0);
        TEST(f << "World\n");
    }
    TEST_EQ(read_file(narrow_filename, data_type::binary), "HelloWorld\n");

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
        create_file(narrow_filename, "Hello");
        {
            nw::fstream f(filename, mode);
            TEST(!f);
        }
        TEST_EQ(read_file(narrow_filename), "Hello");
    }
}

void test_imbue()
{
    boost::nowide::fstream f;
#if BOOST_NOWIDE_USE_FILEBUF_REPLACEMENT
    std::locale convLocale(std::locale::classic(), new dummyCvtConverting);
    TEST_THROW(f.imbue(convLocale), std::runtime_error);
#endif
    std::locale nonconvLocale(std::locale::classic(), new dummyCvtNonConverting);
    f.imbue(nonconvLocale); // No exception, do nothing
}

template<typename T>
void test_open(const T& filename)
{
    const std::string narrow_filename = get_narrow_name(filename);
    remove_file_at_exit _(narrow_filename);

    // Fail on non-existing file
    {
        ensure_not_exists(narrow_filename);
        nw::fstream f;
        f.open(filename);
        TEST(!f);
    }
    TEST(!file_exists(narrow_filename));

    // Create empty file
    {
        ensure_not_exists(narrow_filename);
        nw::fstream f;
        f.open(filename, std::ios::out);
        TEST(f);
    }
    TEST(read_file(narrow_filename).empty());

    // Read+write existing file
    create_file(narrow_filename, "Hello");
    {
        nw::fstream f;
        f.open(filename);
        std::string tmp;
        TEST(f >> tmp);
        TEST(f.eof());
        TEST_EQ(tmp, "Hello");
        f.clear();
        TEST(f << "World");
    }
    TEST_EQ(read_file(narrow_filename), "HelloWorld");

    // Readonly existing file
    create_file(narrow_filename, "Hello");
    {
        nw::fstream f;
        f.open(filename, std::ios::in);
        std::string tmp;
        TEST(f >> tmp);
        TEST_EQ(tmp, "Hello");
        f.clear();
        TEST(f);
        TEST(!(f << "World"));
    }
    TEST_EQ(read_file(narrow_filename), "Hello");

    // remaining mode cases skipped as they are already tested by the ctor tests
}

template<typename T>
bool is_open(T& stream)
{
    // There are const and non const versions of is_open, so test both
    TEST_EQ(stream.is_open(), const_cast<const T&>(stream).is_open());
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
    // Closing again fails
    f.close();
    TEST(!f);
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
    remove_file_at_exit _2(filename2);

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
        TEST_EQ(s, "Foo");

        // new starts where the old was left of
        TEST(f_new);
        TEST(f_new << "World");
    }
    TEST_EQ(read_file(filename), "Hello World");
    TEST_EQ(read_file(filename2), "Foo Bar");

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
            TEST_EQ(s, "ReadThis");
        }
        // new starts where the old was left of
        TEST(f_new);
        TEST(f_new << "World");
    }
    TEST_EQ(read_file(filename), "Hello World");
    TEST_EQ(read_file(filename2), "ReadThis");

    create_file(filename2, "Foo Bar");
    // Swap
    {
        nw::fstream f_old(filename, std::ios::out);
        TEST(f_old << "Hello ");

        nw::fstream f_new(filename2, std::ios::in);
        std::string s;
        TEST(f_new >> s);
        TEST_EQ(s, "Foo");

        // After swapping both are valid and where they left
        f_new.swap(f_old);
        TEST(f_old >> s);
        TEST_EQ(s, "Bar");
        TEST(f_new << "World");

        f_new.close();
        swap(f_new, f_old);
        TEST(!f_old.is_open());
        TEST(f_new.is_open());
    }
    TEST_EQ(read_file(filename), "Hello World");
    TEST_EQ(read_file(filename2), "Foo Bar");
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
            TEST_EQ(s, curValue);
        }
    }
    fo.close();
    TEST(fo.flush());   // Should also work on closed stream
    TEST(!fo.seekg(0)); // Does not work on closed stream
}

void test_main(int, char** argv, char**) // coverity [root_function]
{
    const std::string exampleFilename = std::string(argv[0]) + "-\xd7\xa9-\xd0\xbc-\xce\xbd.txt";

    std::cout << "Ctor" << std::endl;
    test_ctor<const char*>(exampleFilename.c_str());
    test_ctor<std::string>(exampleFilename);
#if BOOST_NOWIDE_USE_WCHAR_OVERLOADS
    test_ctor<const wchar_t*>(boost::nowide::widen(exampleFilename).c_str());
#endif

    std::cout << "Open" << std::endl;
    test_open<const char*>(exampleFilename.c_str());
    test_open<std::string>(exampleFilename);
#if BOOST_NOWIDE_USE_WCHAR_OVERLOADS
    test_open<const wchar_t*>(boost::nowide::widen(exampleFilename).c_str());
#endif

    std::cout << "IsOpen" << std::endl;
    test_is_open(exampleFilename);

    std::cout << "imbue" << std::endl;
    test_imbue();

    std::cout << "Move and swap" << std::endl;
    test_move_and_swap(exampleFilename);

    std::cout << "Flush" << std::endl;
    test_flush(exampleFilename);
}
