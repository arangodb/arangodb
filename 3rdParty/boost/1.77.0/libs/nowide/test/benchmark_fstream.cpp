//
//  Copyright (c) 2012 Artyom Beilis (Tonkikh)
//  Copyright (c) 2019 - 2020 Alexander Grund
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#define BOOST_NOWIDE_TEST_NO_MAIN

#include <boost/nowide/convert.hpp>
#include <boost/nowide/cstdio.hpp>
#include <boost/nowide/fstream.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <vector>

#include "test.hpp"

template<typename Key, typename Value, typename Key2>
Value get(const std::map<Key, Value>& map, const Key2& key)
{
    typename std::map<Key, Value>::const_iterator it = map.find(key);
    if(it == map.end())
        throw std::runtime_error("Key not found");
    return it->second;
}

namespace nw = boost::nowide;
template<typename FStream>
class io_fstream
{
public:
    explicit io_fstream(const char* file, bool binary, bool read)
    {
        auto mode = read ? std::fstream::in : std::fstream::out | std::fstream::trunc;
        if(binary)
            mode |= std::fstream::binary;
        f_.open(file, mode);
        TEST(f_);
    }
    // coverity[exn_spec_violation]
    ~io_fstream()
    {
        f_.close();
    }
    void write(const char* buf, int size)
    {
        TEST(f_.write(buf, size));
    }
    void read(char* buf, int size)
    {
        TEST(f_.read(buf, size));
    }
    void rewind()
    {
        f_.seekg(0);
        f_.seekp(0);
    }
    void flush()
    {
        f_ << std::flush;
    }

private:
    FStream f_;
};

#include <cerrno>

class io_stdio
{
public:
    io_stdio(const char* file, bool binary, bool read)
    {
        const char* mode = read ? "r" : "w+";
        if(binary)
            mode = read ? "rb" : "wb+";
        f_ = nw::fopen(file, mode);
        TEST(f_);
    }
    ~io_stdio()
    {
        std::fclose(f_);
        f_ = 0;
    }
    void write(const char* buf, int size)
    {
        TEST(std::fwrite(buf, 1, size, f_) == static_cast<size_t>(size));
    }
    void read(char* buf, int size)
    {
        TEST(std::fread(buf, 1, size, f_) == static_cast<size_t>(size));
    }
    void rewind()
    {
        std::rewind(f_);
    }
    void flush()
    {
        std::fflush(f_);
    }

private:
    FILE* f_;
};

#if defined(_MSC_VER)
extern "C" void _ReadWriteBarrier(void);
#pragma intrinsic(_ReadWriteBarrier)
#define BOOST_NOWIDE_READ_WRITE_BARRIER() _ReadWriteBarrier()
#elif defined(__GNUC__)
#if(__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) > 40100
#define BOOST_NOWIDE_READ_WRITE_BARRIER() __sync_synchronize()
#else
#define BOOST_NOWIDE_READ_WRITE_BARRIER() __asm__ __volatile__("" : : : "memory")
#endif
#else
#define BOOST_NOWIDE_READ_WRITE_BARRIER() (void)
#endif

using blocksize_to_performance = std::map<size_t, double>;

struct perf_data
{
    // Block-size to read/write performance in MB/s
    blocksize_to_performance read, write;
};

std::vector<char> get_rand_data(int size, bool binary)
{
    std::mt19937 rng{std::random_device{}()};
    auto distr = (binary) ? std::uniform_int_distribution<int>(std::numeric_limits<char>::min(),
                                                               std::numeric_limits<char>::max()) :
                            std::uniform_int_distribution<int>(' ', 'z');
    std::vector<char> data(size);
    std::generate(data.begin(), data.end(), [&]() { return static_cast<char>(distr(rng)); });
    return data;
}

static const int MIN_BLOCK_SIZE = 32;
static const int MAX_BLOCK_SIZE = 8192;

template<typename FStream>
perf_data test_io(const char* file, bool binary)
{
    namespace chrono = std::chrono;
    using clock = chrono::high_resolution_clock;
    using milliseconds = chrono::duration<double, std::milli>;
    perf_data results;
    // Use vector to force write to memory and avoid possible reordering
    std::vector<clock::time_point> start_and_end(2);
    const int data_size = 64 * 1024 * 1024;
    for(int block_size = MIN_BLOCK_SIZE / 2; block_size <= MAX_BLOCK_SIZE; block_size *= 2)
    {
        std::vector<char> buf = get_rand_data(block_size, binary);
        FStream tmp(file, binary, false);
        tmp.rewind();
        start_and_end[0] = clock::now();
        BOOST_NOWIDE_READ_WRITE_BARRIER();
        for(int size = 0; size < data_size; size += block_size)
        {
            tmp.write(&buf[0], block_size);
            BOOST_NOWIDE_READ_WRITE_BARRIER();
        }
        tmp.flush();
        start_and_end[1] = clock::now();
        // heatup
        if(block_size >= MIN_BLOCK_SIZE)
        {
            const milliseconds duration = chrono::duration_cast<milliseconds>(start_and_end[1] - start_and_end[0]);
            const double speed = data_size / duration.count() / 1024; // MB/s
            results.write[block_size] = speed;
            std::cout << "  write block size " << std::setw(8) << block_size << " " << std::fixed
                      << std::setprecision(3) << speed << " MB/s" << std::endl;
        }
    }
    for(int block_size = MIN_BLOCK_SIZE; block_size <= MAX_BLOCK_SIZE; block_size *= 2)
    {
        std::vector<char> buf(block_size);
        FStream tmp(file, binary, true);
        tmp.rewind();
        start_and_end[0] = clock::now();
        BOOST_NOWIDE_READ_WRITE_BARRIER();
        for(int size = 0; size < data_size; size += block_size)
        {
            tmp.read(&buf[0], block_size);
            BOOST_NOWIDE_READ_WRITE_BARRIER();
        }
        start_and_end[1] = clock::now();
        const milliseconds duration = chrono::duration_cast<milliseconds>(start_and_end[1] - start_and_end[0]);
        const double speed = data_size / duration.count() / 1024; // MB/s
        results.read[block_size] = speed;
        std::cout << "  read block size " << std::setw(8) << block_size << " " << std::fixed << std::setprecision(3)
                  << speed << " MB/s" << std::endl;
    }
    TEST(std::remove(file) == 0);
    return results;
}

template<typename FStream>
perf_data test_io_driver(const char* file, const char* type, bool binary)
{
    std::cout << "Testing I/O performance for " << type << std::endl;
    const int repeats = 5;
    std::vector<perf_data> results(repeats);

    for(int i = 0; i < repeats; i++)
        results[i] = test_io<FStream>(file, binary);
    for(int block_size = MIN_BLOCK_SIZE; block_size <= MAX_BLOCK_SIZE; block_size *= 2)
    {
        double read_speed = 0, write_speed = 0;
        for(int i = 0; i < repeats; i++)
        {
            read_speed += get(results[i].read, block_size);
            write_speed += get(results[i].write, block_size);
        }
        results[0].read[block_size] = read_speed / repeats;
        results[0].write[block_size] = write_speed / repeats;
    }
    return results[0];
}

void print_perf_data(const blocksize_to_performance& stdio_data,
                     const blocksize_to_performance& std_data,
                     const blocksize_to_performance& nowide_data)
{
    std::cout << "block size"
              << "     stdio    "
              << " std::fstream "
              << "nowide::fstream" << std::endl;
    for(int block_size = MIN_BLOCK_SIZE; block_size <= MAX_BLOCK_SIZE; block_size *= 2)
    {
        std::cout << std::setw(8) << block_size << "  ";
        std::cout << std::fixed << std::setprecision(3) << std::setw(8) << get(stdio_data, block_size) << " MB/s ";
        std::cout << std::fixed << std::setprecision(3) << std::setw(8) << get(std_data, block_size) << " MB/s ";
        std::cout << std::fixed << std::setprecision(3) << std::setw(8) << get(nowide_data, block_size) << " MB/s ";
        std::cout << std::endl;
    }
}

void test_perf(const char* file)
{
    perf_data nowide_data_txt2 = test_io_driver<io_fstream<nw::fstream>>(file, "std::fstream", false);
    /*
    perf_data stdio_data = test_io_driver<io_stdio>(file, "stdio", true);
    perf_data std_data = test_io_driver<io_fstream<std::fstream>>(file, "std::fstream", true);
    perf_data nowide_data = test_io_driver<io_fstream<nw::fstream>>(file, "nowide::fstream", true);
    perf_data stdio_data_txt = test_io_driver<io_stdio>(file, "stdio", false);
    perf_data std_data_txt = test_io_driver<io_fstream<std::fstream>>(file, "std::fstream", false);
    perf_data nowide_data_txt = test_io_driver<io_fstream<nw::fstream>>(file, "nowide::fstream", false);
    std::cout << "================== Read performance (binary) ==================" << std::endl;
    print_perf_data(stdio_data.read, std_data.read, nowide_data.read);
    std::cout << "================== Write performance (binary) =================" << std::endl;
    print_perf_data(stdio_data.write, std_data.write, nowide_data.write);
    std::cout << "================== Read performance (text) ====================" << std::endl;
    print_perf_data(stdio_data_txt.read, std_data_txt.read, nowide_data_txt.read);
    std::cout << "================== Write performance (text) ===================" << std::endl;
    print_perf_data(stdio_data_txt.write, std_data_txt.write, nowide_data_txt.write);
    */
}

int main(int argc, char** argv)
{
    std::string filename = "perf_test_file.dat";
    if(argc == 2)
    {
        filename = argv[1];
    } else if(argc != 1)
    {
        std::cerr << "Usage: " << argv[0] << " [test_filepath]" << std::endl;
        return 1;
    }
    try
    {
        test_perf(filename.c_str());
    } catch(const std::runtime_error& err)
    {
        std::cerr << "Benchmarking failed: " << err.what() << std::endl;
        return 1;
    }
    return 0;
}
