//
//  Copyright (c) 2020 Alexander Grund
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/nowide/stat.hpp>

#include <boost/nowide/cstdio.hpp>
#include "test.hpp"
#ifdef BOOST_WINDOWS
#include <errno.h>
#endif

void test_main(int, char** argv, char**) // coverity [root_function]
{
    const std::string prefix = argv[0];
    const std::string filename = prefix + "\xd7\xa9-\xd0\xbc-\xce\xbd.txt";

    // Make sure file does not exist
    boost::nowide::remove(filename.c_str());

    std::cout << " -- stat - non-existing file" << std::endl;
    {
#ifdef BOOST_WINDOWS
        struct _stat stdStat;
#else
        struct stat stdStat;
#endif
        TEST(boost::nowide::stat(filename.c_str(), &stdStat) != 0);
        boost::nowide::stat_t boostStat;
        TEST(boost::nowide::stat(filename.c_str(), &boostStat) != 0);
    }

    std::cout << " -- stat - existing file" << std::endl;
    FILE* f = boost::nowide::fopen(filename.c_str(), "wb");
    TEST(f);
    const char testData[] = "Hello World";
    constexpr size_t testDataSize = sizeof(testData);
    TEST_EQ(std::fwrite(testData, sizeof(char), testDataSize, f), testDataSize);
    std::fclose(f);
    {
#ifdef BOOST_WINDOWS
        struct _stat stdStat;
#else
        struct stat stdStat;
#endif /*  */
        TEST_EQ(boost::nowide::stat(filename.c_str(), &stdStat), 0);
        TEST_EQ(static_cast<size_t>(stdStat.st_size), testDataSize);
        boost::nowide::stat_t boostStat;
        TEST_EQ(boost::nowide::stat(filename.c_str(), &boostStat), 0);
        TEST_EQ(static_cast<size_t>(boostStat.st_size), testDataSize);
    }

#ifdef BOOST_WINDOWS
    std::cout << " -- stat - invalid struct size" << std::endl;
    {
        struct _stat stdStat;
        // Simulate passing a struct that is 4 bytes smaller, e.g. if it uses 32 bit time field instead of 64 bit
        // Need to use the detail function directly
        TEST_EQ(boost::nowide::detail::stat(filename.c_str(), &stdStat, sizeof(stdStat) - 4u), EINVAL);
        TEST_EQ(errno, EINVAL);
    }
#endif

    boost::nowide::remove(filename.c_str());
}
