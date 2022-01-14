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

void test_with_different_buffer_sizes(const char* filepath)
{
    /* Important part of the standard for mixing input with output:
       However, output shall not be directly followed by input without an intervening call to the fflush function
       or to a file positioning function (fseek, fsetpos, or rewind),
       and input shall not be directly followed by output without an intervening call to a file positioning function,
       unless the input operation encounters end-of-file.
    */
    for(int i = -1; i < 16; i++)
    {
        std::cout << "Buffer size = " << i << std::endl;
        char buf[16];
        nw::fstream f;
        // Different conditions when setbuf might be called: Usually before opening a file is OK
        if(i >= 0)
            f.rdbuf()->pubsetbuf((i == 0) ? NULL : buf, i);
        f.open(filepath, std::ios::in | std::ios::out | std::ios::trunc | std::ios::binary);
        TEST(f);
        remove_file_at_exit _(filepath);

        // Add 'abcdefg'
        TEST(f.put('a'));
        TEST(f.put('b'));
        TEST(f.put('c'));
        TEST(f.put('d'));
        TEST(f.put('e'));
        TEST(f.put('f'));
        TEST(f.put('g'));
        // Read first char
        TEST(f.seekg(0));
        TEST(f.get() == 'a');
        TEST(f.gcount() == 1u);
        // Skip next char
        TEST(f.seekg(1, std::ios::cur));
        TEST(f.get() == 'c');
        TEST(f.gcount() == 1u);
        // Go back 1 char
        TEST(f.seekg(-1, std::ios::cur));
        TEST(f.get() == 'c');
        TEST(f.gcount() == 1u);

        // Test switching between read->write->read
        // case 1) overwrite, flush, read
        TEST(f.seekg(1));
        TEST(f.put('B'));
        TEST(f.flush()); // Flush when changing out->in
        TEST(f.get() == 'c');
        TEST(f.gcount() == 1u);
        TEST(f.seekg(1));
        TEST(f.get() == 'B');
        TEST(f.gcount() == 1u);
        // case 2) overwrite, seek, read
        TEST(f.seekg(2));
        TEST(f.put('C'));
        TEST(f.seekg(3)); // Seek when changing out->in
        TEST(f.get() == 'd');
        TEST(f.gcount() == 1u);

        // Check that sequence from start equals expected
        TEST(f.seekg(0));
        TEST(f.get() == 'a');
        TEST(f.get() == 'B');
        TEST(f.get() == 'C');
        TEST(f.get() == 'd');
        TEST(f.get() == 'e');

        // Putback after flush is implementation defined
        // Boost.Nowide: Works
#if BOOST_NOWIDE_USE_FILEBUF_REPLACEMENT
        TEST(f << std::flush);
        TEST(f.putback('e'));
        TEST(f.putback('d'));
        TEST(f.get() == 'd');
        TEST(f.get() == 'e');
#endif
        // Rest of sequence
        TEST(f.get() == 'f');
        TEST(f.get() == 'g');
        TEST(f.get() == EOF);

        // Put back until front of file is reached
        f.clear();
        TEST(f.seekg(1));
        TEST(f.get() == 'B');
        TEST(f.putback('B'));
        // Putting back multiple chars is not possible on all implementations after a seek/flush
#if BOOST_NOWIDE_USE_FILEBUF_REPLACEMENT
        TEST(f.putback('a'));
        TEST(!f.putback('x')); // At beginning of file -> No putback possible
        // Get characters that were putback to avoid MSVC bug https://github.com/microsoft/STL/issues/342
        f.clear();
        TEST(f.get() == 'a');
#endif
        TEST(f.get() == 'B');
        f.close();
    }
}

// Reproducer for https://github.com/boostorg/nowide/issues/126
void test_getline_and_tellg(const char* filename)
{
    {
        nw::ofstream f(filename);
        f << "Line 1" << std::endl;
        f << "Line 2" << std::endl;
        f << "Line 3" << std::endl;
    }
    remove_file_at_exit _(filename);
    nw::fstream f;
    // Open file in text mode, to read
    f.open(filename, std::ios_base::in);
    TEST(f);
    std::string line1, line2, line3;
    TEST(getline(f, line1));
    TEST(line1 == "Line 1");
    const auto tg = f.tellg(); // This may cause issues
    TEST(tg > 0u);
    TEST(getline(f, line2));
    TEST(line2 == "Line 2");
    TEST(getline(f, line3));
    TEST(line3 == "Line 3");
}

// Test that a sync after a peek does not swallow newlines
// This can happen because peek reads a char which needs to be "unread" on sync which may loose a converted newline
void test_peek_sync_get(const char* filename)
{
    {
        nw::ofstream f(filename);
        f << "Line 1" << std::endl;
        f << "Line 2" << std::endl;
    }
    remove_file_at_exit _(filename);
    nw::ifstream f(filename);
    TEST(f);
    while(f)
    {
        const int curChar = f.peek();
        if(curChar == std::char_traits<char>::eof())
            break;
        f.sync();
        TEST(f.get() == char(curChar));
    }
}

/// Test swapping at many possible positions within a stream to shake out missed state
void test_swap(const char* filename, const char* filename2)
{
    remove_file_at_exit _(filename);
    remove_file_at_exit _2(filename2);

    {
        nw::ofstream f(filename);
        f << create_random_data(BUFSIZ * 2, data_type::text);
        f.close();
        f.open(filename2);
        f << create_random_data(BUFSIZ * 3, data_type::text);
    }

    nw::ifstream f1(filename);
    nw::ifstream f2(filename2);
    TEST(f1);
    TEST(f2);
    unsigned ctr = 0;
    while(f1 && f2)
    {
        const int curChar1 = f1.peek();
        const int curChar2 = f2.peek();
        // Randomly do a no-op seek of either or both streams to flush internal buffer
        if(ctr % 10 == 0)
            TEST(f1.seekg(f1.tellg()));
        else if(ctr % 15 == 0)
            TEST(f2.seekg(f2.tellg()));
        f1.swap(f2);
        TEST(f1.peek() == curChar2);
        TEST(f2.peek() == curChar1);
        if(ctr % 10 == 4)
            TEST(f1.seekg(f1.tellg()));
        else if(ctr % 15 == 4)
            TEST(f2.seekg(f2.tellg()));
        TEST(f1.get() == char(curChar2));
        f1.swap(f2);
        TEST(f1.get() == char(curChar1));
        ++ctr;
    }
}

// coverity [root_function]
void test_main(int, char** argv, char**)
{
    const std::string exampleFilename = std::string(argv[0]) + "-\xd7\xa9-\xd0\xbc-\xce\xbd.txt";
    const std::string exampleFilename2 = std::string(argv[0]) + "-\xd7\xa9-\xd0\xbc-\xce\xbd 2.txt";

    std::cout << "Complex IO" << std::endl;
    test_with_different_buffer_sizes(exampleFilename.c_str());

    std::cout << "Regression tests" << std::endl;
    test_getline_and_tellg(exampleFilename.c_str());
    test_peek_sync_get(exampleFilename.c_str());
    test_swap(exampleFilename.c_str(), exampleFilename2.c_str());
}
