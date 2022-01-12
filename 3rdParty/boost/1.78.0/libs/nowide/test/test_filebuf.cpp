//  Copyright (c) 2015 Artyom Beilis (Tonkikh)
//  Copyright (c) 2019-2021 Alexander Grund
//
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/nowide/filebuf.hpp>

#include "file_test_helpers.hpp"
#include "test.hpp"
#include <cstdint>
#include <random>
#include <string>
#include <type_traits>

namespace nw = boost::nowide;
using namespace boost::nowide::test;

// Check member types
static_assert(std::is_same<nw::filebuf::char_type, char>::value, "!");
static_assert(std::is_same<nw::filebuf::traits_type::char_type, char>::value, "!");
static_assert(std::is_same<nw::filebuf::int_type, nw::filebuf::traits_type::int_type>::value, "!");
static_assert(std::is_same<nw::filebuf::pos_type, nw::filebuf::traits_type::pos_type>::value, "!");
static_assert(std::is_same<nw::filebuf::off_type, nw::filebuf::traits_type::off_type>::value, "!");

void test_open_close(const std::string& filepath)
{
    const std::string filepath2 = filepath + ".2";
    ensure_not_exists(filepath2);
    remove_file_at_exit _(filepath);
    remove_file_at_exit _2(filepath2);

    nw::filebuf buf;
    TEST(buf.open(filepath, std::ios_base::out) == &buf);
    TEST(buf.is_open());

    // Opening when already open fails
    TEST(buf.open(filepath2, std::ios_base::out) == nullptr);
    // Still open
    TEST(buf.is_open());
    TEST(buf.close() == &buf);
    // Failed opening did not create file
    TEST(!file_exists(filepath2));

    // But it should work now:
    TEST(buf.open(filepath2, std::ios_base::out) == &buf);
    TEST(buf.close() == &buf);
    TEST(file_exists(filepath2));
}

void test_pubseekpos(const std::string& filepath)
{
    const std::string data = create_random_data(BUFSIZ * 4, data_type::binary);
    create_file(filepath, data, data_type::binary);
    nw::filebuf buf;
    TEST(buf.open(filepath, std::ios_base::in | std::ios_base::binary) == &buf);

    // Fuzzy test: Seek to a couple random positions
    std::minstd_rand rng(std::random_device{}());
    using pos_type = nw::filebuf::pos_type;
    const auto eofPos = pos_type(data.size());
    std::uniform_int_distribution<size_t> distr(0, static_cast<size_t>(eofPos) - 1);
    using traits = nw::filebuf::traits_type;

    const auto getData = [&](pos_type pos) { return traits::to_int_type(data[static_cast<size_t>(pos)]); };

    for(int i = 0; i < 100; i++)
    {
        const pos_type pos = distr(rng);
        TEST_EQ(buf.pubseekpos(pos), pos);
        TEST_EQ(buf.sgetc(), getData(pos));
    }
    // Seek to first and last as corner case tests
    TEST_EQ(buf.pubseekpos(0), pos_type(0));
    TEST_EQ(buf.sgetc(), traits::to_int_type(data[0]));
    TEST_EQ(buf.pubseekpos(eofPos), eofPos);
    TEST_EQ(buf.sgetc(), traits::eof());
}

void test_pubseekoff(const std::string& filepath)
{
    const std::string data = create_random_data(BUFSIZ * 4, data_type::binary);
    create_file(filepath, data, data_type::binary);
    nw::filebuf buf;
    TEST(buf.open(filepath, std::ios_base::in | std::ios_base::binary) == &buf);

    // Fuzzy test: Seek to a couple random positions
    std::minstd_rand rng(std::random_device{}());
    using pos_type = nw::filebuf::pos_type;
    using off_type = nw::filebuf::off_type;
    const auto eofPos = pos_type(data.size());
    std::uniform_int_distribution<size_t> distr(0, static_cast<size_t>(eofPos) - 1);
    using traits = nw::filebuf::traits_type;

    const auto getData = [&](pos_type pos) { return traits::to_int_type(data[static_cast<size_t>(pos)]); };
    // tellg/tellp function as called by basic_[io]fstream
    const auto tellg = [&]() { return buf.pubseekoff(0, std::ios_base::cur); };

    for(int i = 0; i < 100; i++)
    {
        // beg
        pos_type pos = distr(rng);
        TEST_EQ(buf.pubseekoff(pos, std::ios_base::beg), pos);
        TEST_EQ(tellg(), pos);
        TEST_EQ(buf.sgetc(), getData(pos));
        // cur
        off_type diff = static_cast<pos_type>(distr(rng)) - pos;
        pos += diff;
        TEST_EQ(buf.pubseekoff(diff, std::ios_base::cur), pos);
        TEST_EQ(tellg(), pos);
        TEST_EQ(buf.sgetc(), getData(pos));
        // end
        diff = static_cast<pos_type>(distr(rng)) - eofPos;
        pos = eofPos + diff;
        TEST_EQ(buf.pubseekoff(diff, std::ios_base::end), pos);
        TEST_EQ(tellg(), pos);
        TEST_EQ(buf.sgetc(), getData(pos));
    }
    // Seek to first and last as corner case tests
    TEST_EQ(buf.pubseekoff(0, std::ios_base::beg), pos_type(0));
    TEST_EQ(tellg(), pos_type(0));
    TEST_EQ(buf.sgetc(), traits::to_int_type(data[0]));
    TEST_EQ(buf.pubseekoff(0, std::ios_base::end), eofPos);
    TEST_EQ(tellg(), eofPos);
    TEST_EQ(buf.sgetc(), traits::eof());
}

void test_64_bit_seek(const std::string& filepath)
{
    // Create a value which does not fit into a 32 bit value.
    // Use an unsigned intermediate to have the truncation defined to wrap to 0
    using unsigned_off_type = std::make_unsigned<nw::filebuf::off_type>::type;
    nw::filebuf::off_type offset = static_cast<unsigned_off_type>(std::uint64_t(1) << 33u);

#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable : 4127)
#endif
    // if we can't use 64 bit offsets through the API, don't test anything
    // LCOV_EXCL_START
    // coverity[result_independent_of_operands]
    if(offset == nw::filebuf::off_type(0))
    {
        return; // coverity[dead_error_line]
    }
    // LCOV_EXCL_STOP
#ifdef BOOST_MSVC
#pragma warning(pop)
#endif

    create_file(filepath, "test");
    remove_file_at_exit _(filepath);

    nw::filebuf buf;
    TEST(buf.open(filepath, std::ios_base::in | std::ios_base::binary) == &buf);
    const std::streampos knownPos = 2;
    TEST_EQ(buf.pubseekpos(knownPos), knownPos); // Just to make sure we know where we are
    const std::streampos newPos = buf.pubseekoff(offset, std::ios_base::cur);
    // On 32 bit mode or when seek beyond EOF is not allowed, the current position should be unchanged
    if(newPos == std::streampos(-1))
        TEST_EQ(buf.pubseekoff(0, std::ios_base::cur), knownPos);
    else
    {
#if !BOOST_NOWIDE_USE_FILEBUF_REPLACEMENT
        // libc++ may truncate the 64 bit value when calling fseek which yields an offset of 0
        if(newPos == knownPos)
            offset = 0; // LCOV_EXCL_LINE
#endif
        TEST_EQ(newPos, offset + knownPos);
        TEST_EQ(buf.pubseekoff(0, std::ios_base::cur), newPos);
    }
}

void test_swap(const std::string& filepath)
{
    const std::string filepath2 = filepath + ".2";
    remove_file_at_exit _(filepath);
    remove_file_at_exit _2(filepath2);

    const auto eof = nw::filebuf::traits_type::eof();
    // Note: Make sure to have en uneven number of swaps so the destructor runs on the others data

    // Check: FILE*, buffer, buffer_size
    {
        nw::filebuf buf1, buf2;
        char buffer1[3]{}, buffer2[5]{};
        buf1.pubsetbuf(buffer1, sizeof(buffer1));
        buf2.pubsetbuf(buffer2, sizeof(buffer2));
        TEST(buf1.open(filepath, std::ios_base::out) == &buf1);
        buf1.swap(buf2);
        TEST(!buf1.is_open());
        TEST(buf2.is_open());
        TEST(buf1.open(filepath2, std::ios_base::out | std::ios_base::binary) == &buf1);

        // Write "FooBar" to filepath and "HelloWorld" to filepath2
        buf1.sputc('H');
        buf1.sputn("ello", 4);
        buf2.sputc('F');
        buf2.sputn("oo", 2);
        buf2.swap(buf1);
        buf1.sputc('B');
        buf1.sputn("ar", 2);
        buf2.sputc('W');
        buf2.sputn("orld", 4);

        buf1.close();
        TEST(!buf1.is_open());
        TEST(buf2.is_open());
        buf1.swap(buf2);
        TEST(buf1.is_open());
        TEST(!buf2.is_open());
        buf1.close();
        TEST(!buf1.is_open());
        TEST(!buf2.is_open());
        TEST_EQ(read_file(filepath), "FooBar");
        TEST_EQ(read_file(filepath2), "HelloWorld");
    }
    // Check: mode, owns_buffer
    {
        nw::filebuf buf1, buf2;
        char buffer[3]{};
        buf1.pubsetbuf(buffer, sizeof(buffer));
        TEST(buf1.open(filepath, std::ios_base::out) == &buf1);
        TEST(buf2.open(filepath2, std::ios_base::in) == &buf2);
        TEST_EQ(buf1.sputc('B'), 'B');
        TEST_EQ(buf2.sbumpc(), 'H');
        buf1.swap(buf2);
        // Trying to read in write mode or other way round should fail
        TEST_EQ(buf1.sputc('x'), eof);
        TEST_EQ(buf2.sbumpc(), eof);
        TEST_EQ(buf1.sbumpc(), 'e');
        TEST_EQ(buf2.sputc('a'), 'a');
        buf2.swap(buf1);
        TEST_EQ(buf2.sputc('x'), eof);
        TEST_EQ(buf1.sbumpc(), eof);
        TEST_EQ(buf2.sbumpc(), 'l');
        TEST_EQ(buf1.sputn("zXYZ", 4), 4);
        swap(buf2, buf1);
        buf1.close();
        buf2.close();
        TEST_EQ(read_file(filepath), "BazXYZ");
        TEST_EQ(read_file(filepath2), "HelloWorld");
    }
    // Check: last_char, gptr, eback
    {
        nw::filebuf buf1, buf2;
        // Need to disable buffering to use last_char, but only for 1 to detect wrong conditions
        buf1.pubsetbuf(0, 0);
        TEST(buf1.open(filepath, std::ios_base::in) == &buf1);
        TEST(buf2.open(filepath2, std::ios_base::in) == &buf2);
        // Peek
        TEST_EQ(buf1.sgetc(), 'B');
        TEST_EQ(buf2.sgetc(), 'H');
        swap(buf1, buf2);
        TEST_EQ(buf2.sgetc(), 'B');
        TEST_EQ(buf1.sgetc(), 'H');
        // Advance
        TEST_EQ(buf2.sbumpc(), 'B');
        TEST_EQ(buf1.sbumpc(), 'H');
        TEST_EQ(buf2.sbumpc(), 'a');
        TEST_EQ(buf1.sbumpc(), 'e');
        swap(buf1, buf2);
        TEST_EQ(buf1.sbumpc(), 'z');
        TEST_EQ(buf2.sbumpc(), 'l');
        swap(buf1, buf2);
        TEST_EQ(buf2.sgetc(), 'X');
        TEST_EQ(buf1.sgetc(), 'l');
    }
    // Check: pptr, epptr
    {
        nw::filebuf buf1, buf2;
        // Need to disable buffering to use last_char, but only for 1 to detect wrong conditions
        buf1.pubsetbuf(0, 0);
        TEST(buf1.open(filepath, std::ios_base::out) == &buf1);
        TEST(buf2.open(filepath2, std::ios_base::out) == &buf2);
        TEST_EQ(buf1.sputc('1'), '1');
        TEST_EQ(buf2.sputc('a'), 'a');
        swap(buf1, buf2);
        // buf1: filepath2, buf2: filepath
        TEST_EQ(buf1.sputc('b'), 'b');
        TEST_EQ(buf2.sputc('2'), '2');
        // Sync and check if file was written
        TEST_EQ(buf1.pubsync(), 0);
        TEST_EQ(read_file(filepath2), "ab");
        TEST_EQ(buf2.pubsync(), 0);
        TEST_EQ(read_file(filepath), "12");
        swap(buf1, buf2);
        // buf1: filepath, buf2: filepath2
        TEST_EQ(buf1.pubsync(), 0);
        TEST_EQ(read_file(filepath), "12");
        TEST_EQ(buf2.pubsync(), 0);
        TEST_EQ(read_file(filepath2), "ab");
        TEST_EQ(buf1.sputc('3'), '3');
        TEST_EQ(buf2.sputc('c'), 'c');
        swap(buf1, buf2);
        // buf1: filepath2, buf2: filepath
        TEST_EQ(buf1.pubsync(), 0);
        TEST_EQ(read_file(filepath2), "abc");
        TEST_EQ(buf2.pubsync(), 0);
        TEST_EQ(read_file(filepath), "123");
    }
}

void test_main(int, char** argv, char**) // coverity [root_function]
{
    const std::string exampleFilename = std::string(argv[0]) + "-\xd7\xa9-\xd0\xbc-\xce\xbd.txt";

    test_open_close(exampleFilename);
    test_pubseekpos(exampleFilename);
    test_pubseekoff(exampleFilename);
    test_64_bit_seek(exampleFilename);
// These tests are only useful for the nowide filebuf and are known to fail for
// std::filebuf due to bugs in libc++
#if BOOST_NOWIDE_USE_FILEBUF_REPLACEMENT
    test_swap(exampleFilename);
#endif
}
