//
// Created by kleme on 26.02.2018.
//

#define BOOST_ASIO_NO_DEPRECATED 1
#include <boost/process.hpp>
int main() {}

#if defined(BOOST_POSIX_API)
#include <boost/process/posix.hpp>
#else
#include <boost/process/windows.hpp>
#endif