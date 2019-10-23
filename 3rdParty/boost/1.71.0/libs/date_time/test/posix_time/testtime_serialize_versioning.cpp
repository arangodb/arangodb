/* Copyright (c) 2017 James E. King III
 * Use, modification and distribution is subject to the
 * Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)
 */
 
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/time_serialize.hpp>
#include <boost/lexical_cast.hpp>
#include "../testfrmwk.hpp"
#include <fstream>

using namespace boost;
using namespace posix_time;

void check_filesize(const std::string& filename, std::ifstream::pos_type expectedSize)
{
    std::ifstream in(filename.c_str(), std::ifstream::ate | std::ifstream::binary);
    check_equal("check file size is " + boost::lexical_cast<std::string>(expectedSize), in.tellg(), expectedSize);
}

std::string get_fname(int version)
{
    return "time_duration_serialization." +
        std::string((sizeof(size_t) == 4) ? "x32" : "x64") +
        ".v" + boost::lexical_cast<std::string>(version);
}

int main() {
    time_duration td(12, 13, 52, 123456);

#if BOOST_DATE_TIME_POSIX_TIME_DURATION_VERSION == 0
    std::ofstream ofs(get_fname(0).c_str(), std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);
    boost::archive::binary_oarchive oa(ofs);
    oa << td;
    ofs.close();

#if defined(_MSC_VER)
    check_filesize(get_fname(0), 58 + sizeof(size_t));
#endif

#else // BOOST_DATE_TIME_POSIX_TIME_DURATION_VERSION > 0
    std::ifstream ifs(get_fname(0).c_str(), std::ios_base::binary | std::ios_base::in);
    boost::archive::binary_iarchive ia(ifs);
    time_duration tmp;
    ia >> tmp;
    ifs.close();
    check_equal("read older version structure ok", td, tmp);

    std::ofstream ofs(get_fname(1).c_str(), std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);
    boost::archive::binary_oarchive oa(ofs);
    oa << td;
    ofs.close();

#if defined(_MSC_VER)
    check_filesize(get_fname(1), 70 + sizeof(size_t));
#endif

#endif // BOOST_DATE_TIME_POSIX_TIME_DURATION_VERSION

    return printTestStats();
}
