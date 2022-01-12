//  Copyright (c) 2015 Artyom Beilis (Tonkikh)
//  Copyright (c) 2020 - 2021 Alexander Grund
//
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _SCL_SECURE_NO_WARNINGS
// Call to 'std::copy_n' with parameters that may be unsafe
#define _SCL_SECURE_NO_WARNINGS
#endif
#include <boost/nowide/iostream.hpp>

#include <boost/nowide/convert.hpp>
#include <boost/nowide/utf/utf.hpp>
#include "../src/console_buffer.hpp"
#include "file_test_helpers.hpp"
#include "test.hpp"
#include "test_sets.hpp"
#include <algorithm>
#include <fstream>
#include <limits>
#include <queue>
#include <string>

namespace nw = boost::nowide;

const std::string outputString =
  // German umlauts (aou with 2 dots), cyrillic small m, greek small nu
  "Basic letters: \xc3\xa4-\xc3\xb6-\xc3\xbc-\xd0\xbc-\xce\xbd\n"
  "East Asian Letters: \xe5\x92\x8c-\xe5\xb9\xb3\n"
  "Non-BMP letter: \xf0\x9d\x84\x9e\n" // musical symbol g clef
  "Invalid UTF-8: `\xFF' `\xd7\xFF' `\xe5\xFF\x8c' `\xf0\x9d\x84\xFF' \n"
  "\n";

const bool usesNowideRdBufIn = nw::cin.rdbuf() != std::cin.rdbuf();
const bool usesNowideRdBufOut = nw::cout.rdbuf() != std::cout.rdbuf();

#ifndef BOOST_NOWIDE_TEST_INTERACTIVE
class mock_output_buffer final : public nw::detail::console_output_buffer_base
{
public:
    std::wstring output;
    bool succeed = true;

protected:
    bool do_write(const wchar_t* buffer, std::size_t num_chars_to_write, std::size_t& num_chars_written) override
    {
        if(succeed)
        {
            output.insert(output.end(), buffer, buffer + num_chars_to_write);
            num_chars_written = num_chars_to_write;
            return true;
        } else
        {
            num_chars_written = 0;
            return false;
        }
    }
};

class mock_input_buffer final : public nw::detail::console_input_buffer_base
{
public:
    std::queue<std::wstring> inputs;

protected:
    bool do_read(wchar_t* buffer, std::size_t num_chars_to_read, std::size_t& num_chars_read) override
    {
        if(inputs.empty())
            return false;
        std::wstring& input = inputs.front();
        num_chars_read = std::min(num_chars_to_read, input.size());
        std::copy_n(input.begin(), num_chars_read, buffer);
        input.erase(input.begin(), input.begin() + num_chars_read);
        if(input.empty())
            inputs.pop();
        return true;
    }
};

/// Scoped change of a streams rdbuf
struct scoped_rdbuf_change
{
    std::ios& stream;
    std::streambuf* orig_buf;
    scoped_rdbuf_change(std::ios& stream, std::streambuf* new_buf) : stream(stream), orig_buf(stream.rdbuf(new_buf))
    {}
    ~scoped_rdbuf_change()
    {
        stream.rdbuf(orig_buf);
    }
};

// Macros to be used to avoid litering the code with #ifndef checks
/// Install a mock buffer into the given stream, when compiling as non-interactive
#define INSTALL_MOCK_BUF(STREAM, BUF_TYPE) \
    BUF_TYPE mock_buf;                     \
    scoped_rdbuf_change _(nw::STREAM, &mock_buf)
/// Run the given cmd(s) only when compiling as non-interactive
#define RUN_MOCKED(what) what
#else
#define INSTALL_MOCK_BUF(STREAM, BUF_TYPE)
#define RUN_MOCKED(what)
#endif
/// Assert the given condition/code only when compiling as non-interactive
#define TEST_MOCKED(what) RUN_MOCKED(TEST(what))

bool is_valid_UTF8(const std::string& s)
{
    using namespace boost::nowide::utf;
    for(std::string::const_iterator it = s.begin(); it != s.end();)
    {
        code_point c = utf_traits<char>::decode(it, s.end());
        if(!is_valid_codepoint(c))
            return false;
    }
    return true;
}

std::string create_random_one_line_string(std::size_t num_chars)
{
    std::string result = nw::test::create_random_data(num_chars, boost::nowide::test::data_type::text);
    // Make sure it is a single line
    std::replace(result.begin(), result.end(), '\n', 'a');
    return result;
}

void test_is_valid_UTF8()
{
    // Sanity check of the test function
    TEST(is_valid_UTF8(""));                                 // Empty string is valid by definition
    TEST(is_valid_UTF8(create_random_one_line_string(100))); // ASCII string is valid
    TEST(is_valid_UTF8(roundtrip_tests[5].utf8));            //  UTF-8 string is valid
    TEST(!is_valid_UTF8(invalid_utf8_tests[0].utf8));        // Detect invalid
}

void test_tie()
{
    TEST(nw::cin.tie() == &nw::cout);
    TEST(nw::cerr.tie() == &nw::cout);
    TEST((nw::cerr.flags() & std::ios_base::unitbuf) != 0);
    TEST(nw::clog.tie() == nullptr);
}

void test_putback_and_get()
{
    // If we are using the standard rdbuf we can only put back 1 char
    // This should always work
    int maxval = 15000;
    for(int i = 0; i < maxval; i++)
    {
        char c = i % 96 + ' ';
        TEST(nw::cin.putback(c));
        int ci = i % 96 + ' ';
        TEST_EQ(nw::cin.get(), ci);
    }

    INSTALL_MOCK_BUF(cin, mock_input_buffer);
    if(usesNowideRdBufIn RUN_MOCKED(|| true))
    {
        // Test with a few small values and around power-of-2 values as buffer size doubles.
        // Finally test a large value
        for(const int num_putback_chars : {1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 1000})
        {
            const auto getChar = [&](int i) { return (i + num_putback_chars) % 96 + ' '; };
            for(int i = 0; i < num_putback_chars; i++)
            {
                const char c = static_cast<char>(getChar(i));
                TEST(nw::cin.putback(c));
            }
            for(int i = num_putback_chars - 1; i >= 0; i--)
            {
                const int c = getChar(i);
                TEST_EQ(nw::cin.get(), c);
            }
            // Check unget (all chars)
            for(int i = 0; i < num_putback_chars; i++)
                TEST(nw::cin.unget());
            TEST(!nw::cin.unget());
            nw::cin.clear();
            for(int i = num_putback_chars - 1; i >= 0; i--)
            {
                const int c = getChar(i);
                TEST_EQ(nw::cin.get(), c);
            }
        }
#ifndef BOOST_NOWIDE_TEST_INTERACTIVE
        // Putback 1 char, then get the rest from "real" input
        nw::cin.putback('T');
        mock_buf.inputs.push(L"est\r\n");
        std::string test;
        TEST(nw::cin >> test);
        TEST_EQ(test, "Test");
#endif
    }
}

void test_cout()
{
    INSTALL_MOCK_BUF(cout, mock_output_buffer);
    TEST(nw::cout);
    TEST(nw::cout << outputString << std::endl);
    TEST_MOCKED(mock_buf.output == nw::widen(outputString + "\n"));
#ifndef BOOST_NOWIDE_TEST_INTERACTIVE
    // Pretend the actual write to console fails
    mock_buf.output.clear();
    mock_buf.succeed = false;
    TEST(!(nw::cout << "Fail this" << std::endl));
    TEST(mock_buf.output.empty());
#endif
}

void test_cout_single_char()
{
    INSTALL_MOCK_BUF(cout, mock_output_buffer);
    for(const char s : outputString)
        TEST(nw::cout << s << std::flush);
    TEST(nw::cout);
    TEST_MOCKED(mock_buf.output == nw::widen(outputString));
}

void test_cerr()
{
    INSTALL_MOCK_BUF(cerr, mock_output_buffer);
    TEST(nw::cerr);
    TEST(nw::cerr << outputString << std::endl);
    TEST_MOCKED(mock_buf.output == nw::widen(outputString + "\n"));
    // Output to cerr is unbuffered and should be flushed for every single output
    RUN_MOCKED(mock_buf.output.clear());
    TEST_MOCKED(nw::cerr << "a");
    TEST_MOCKED(mock_buf.output == nw::widen("a"));
    TEST_MOCKED(nw::cerr << "Hello World");
    TEST_MOCKED(mock_buf.output == nw::widen("aHello World"));
}

void test_cerr_single_char()
{
    INSTALL_MOCK_BUF(cerr, mock_output_buffer);
    for(const char s : outputString)
        TEST(nw::cerr << s << std::flush);
    TEST(nw::cerr);
    TEST_MOCKED(mock_buf.output == nw::widen(outputString));
}

void test_cin()
{
    RUN_MOCKED(const std::wstring lineBreak = L"\r\n"; const std::wstring space = L" ");
    INSTALL_MOCK_BUF(cin, mock_input_buffer);
    nw::cout << "Input 2 strings, e.g. 'Hello World<ENTER>'" << std::endl;
    static_assert(array_size(roundtrip_tests) > 7, "!");
    RUN_MOCKED(mock_buf.inputs.push(roundtrip_tests[6].wide + space + roundtrip_tests[7].wide + lineBreak));
    std::string v1, v2;
    nw::cin >> v1 >> v2;
    TEST(nw::cin);
    TEST(is_valid_UTF8(v1));
    TEST(is_valid_UTF8(v2));
    TEST(nw::cout << "First:  " << v1 << std::endl);
    TEST(nw::cout << "Second: " << v2 << std::endl);
    TEST_MOCKED(v1 == roundtrip_tests[6].utf8);
    TEST_MOCKED(v2 == roundtrip_tests[7].utf8);

    // Check sync
    nw::cout << "Input 2 strings, e.g. 'Two more<ENTER>'" << std::endl;
    RUN_MOCKED(mock_buf.inputs.push(L"First_String\u00F1" + space + L"Second_String_Ignored" + lineBreak));
    TEST(nw::cin >> v1);
    nw::cin.sync();
    nw::cout << "The 2nd string should have been ignored. Input 1 more + [ENTER]" << std::endl;
    RUN_MOCKED(mock_buf.inputs.push(L"Third_\xDC01_String" + lineBreak)); // Note: Invalid UTF-16
    TEST(nw::cin >> v2);
    TEST(!v2.empty());
    nw::cout << "First:  " << v1 << std::endl;
    nw::cout << "Second:  " << v2 << std::endl;
    TEST_MOCKED(v1 == "First_String\xc3\xb1");
    TEST_MOCKED(v2 == "Third_\xEF\xBF\xBD_String");
}

void test_cin_getline()
{
    INSTALL_MOCK_BUF(cin, mock_input_buffer);
    std::string value;
    for(int i = 0; i < 10; i++)
    {
        nw::cout << "Input a line of text or simply press ENTER to exit, e.g. 'Hello World to you!<ENTER>'"
                 << std::endl;
        // Add a longish string which eventually exceeds the buffer size of the console_buffer
        RUN_MOCKED(const std::string expected = (i == 9) ? "" : create_random_one_line_string(i * 211 + 13));
        // Convert to wstring and push (we can do this as the chars are ASCII)
        RUN_MOCKED(mock_buf.inputs.push(std::wstring(expected.begin(), expected.end()) + L"\r\n"));
        TEST(std::getline(nw::cin, value));
        if(value.empty())
        {
            TEST_MOCKED(i == 9);
            nw::cout << "END\n";
            break;
        }
        TEST_MOCKED(i != 9);
        // It should not include the CR
        TEST(value.back() != '\r');
        nw::cout << i << ": " << value << std::endl;
        TEST_MOCKED(value == expected);
    }
#ifndef BOOST_NOWIDE_TEST_INTERACTIVE
    TEST(!std::getline(nw::cin, value));
    TEST(nw::cin.eof());
    TEST(value.empty());
    nw::cin.clear();
    // Check that incomplete chars at the buffer border are handled correctly by creating a very long string
    std::wstring input;
    constexpr std::size_t buffer_size = 1024; // From console_buffer_base
    input.reserve(buffer_size * 4);
    input.append(buffer_size - 2, L'a'); // 2 chars before end of buffer
    input.append(L"\U0001033C");         // 2 UTF-16 chars --> Buffer full
    input.append(buffer_size - 1, L'a'); // Just before end of buffer
    input.append(L"\U0001033C");         // over buffer boundary
    input.append(L"\U0001033Ca");        // in new buffer and align again
    // Fill up with largest code point
    for(std::size_t i = 0; i < buffer_size; i++)
        input.append(L"\U0010FFFF");
    mock_buf.inputs.push(input + L"\r\n");
    TEST(std::getline(nw::cin, value));
    TEST_EQ(value, nw::narrow(input));
#endif
}

void test_ctrl_z_is_eof()
{
    INSTALL_MOCK_BUF(cin, mock_input_buffer);
    std::string value;
    nw::cout << "Input a line of text and then press CTRL+Z, e.g. 'Hello World!<ENTER><CTRL+Z><ENTER>'" << std::endl;
    RUN_MOCKED(mock_buf.inputs.push(L"Hello World!\r\n"));
    RUN_MOCKED(mock_buf.inputs.push(L"\x1a\r\n"));
    RUN_MOCKED(mock_buf.inputs.push(L"Reached after clear()\r\n"));
    TEST(std::getline(nw::cin, value));
    // It should not include the CR
    TEST(!value.empty());
    TEST(value.back() != '\r');
    nw::cout << "Value: " << value << std::endl;
    TEST_MOCKED(value == "Hello World!");
    TEST(!std::getline(nw::cin, value));
    TEST(nw::cin.eof());
    nw::cin.clear();
    nw::cout << "clear() called, input another line, e.g. 'Hi there!<ENTER>'" << std::endl;
    TEST(std::getline(nw::cin, value));
    // It should not include the CR
    TEST(!value.empty());
    TEST(value.back() != '\r');
    nw::cout << "Value: " << value << std::endl;
    TEST_MOCKED(value == "Reached after clear()");
#ifndef BOOST_NOWIDE_TEST_INTERACTIVE
    // CTRL+Z anywhere else but at the start of a line does not matter
    for(int i = 1; i <= 1100; i++)
    {
        const std::string expected = create_random_one_line_string(i) + "\x1a";
        mock_buf.inputs.push(std::wstring(expected.begin(), expected.end()) + L"\r\n");
        TEST(std::getline(nw::cin, value));
        TEST_EQ(value, expected);
    }
#endif
}

void test_main(int argc, char** argv, char**) // coverity [root_function]
{
    // LCOV_EXCL_START
    if(usesNowideRdBufIn)
        nw::cout << "Using Nowide input buffer\n";
    else
        nw::cout << "NOT using Nowide input buffer\n";
    if(usesNowideRdBufOut)
        nw::cout << "Using Nowide output buffer\n"; // LCOV_EXCL_LINE
    else
        nw::cout << "NOT using Nowide output buffer\n";
    // LCOV_EXCL_STOP

    const std::string arg = (argc == 1) ? "" : argv[1];
    if(arg == "passthrough") // Read string from cin and write to cout
    {
        // Check that input and output are not using the nowide filebufs as expected when the consoles are redirected
        TEST(!usesNowideRdBufIn);
        TEST(!usesNowideRdBufOut);
        std::string s;
        TEST(std::getline(nw::cin, s));
        TEST(nw::cout << s);
        return;
    }

#ifdef BOOST_NOWIDE_TEST_INTERACTIVE
    // LCOV_EXCL_START
    nw::cout << "Output different chars:" << std::endl;
    test_cout();
    nw::cout << "Same again:" << std::endl;
    test_cout_single_char();

    nw::cout << "Same 2 outputs but to stderr:" << std::endl;
    test_cerr();
    test_cerr_single_char();

    nw::cout << "Basic cin tests:" << std::endl;
    test_cin();

    nw::cout << "getline test:" << std::endl;
    nw::cin.ignore(std::numeric_limits<int>::max(), '\n'); // Clear newline from last test
    test_cin_getline();

    nw::cout << "CTRL+Z test:" << std::endl;
    test_ctrl_z_is_eof();
    // LCOV_EXCL_STOP
#else
    test_is_valid_UTF8();
    test_tie();
    test_putback_and_get();
    test_cout();
    test_cout_single_char();
    test_cerr();
    test_cerr_single_char();
    test_cin();
    test_cin_getline();
    test_ctrl_z_is_eof();
#endif // BOOST_NOWIDE_TEST_INTERACTIVE
}
