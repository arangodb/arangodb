//  error_code_test.cpp  ---------------------------------------------------------------//

//  Copyright Beman Dawes 2014

//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt

//  See library home page at http://www.boost.org/libs/system

#include <boost/system/config.hpp>
#include <iostream>

using std::cout;
using std::endl;

int main()
{
#ifdef BOOST_WINDOWS_API
  std::cout << "BOOST_WINDOWS_API is defined" << std::endl;
#else
  std::cout << "BOOST_WINDOWS_API is not defined" << std::endl;
#endif
#ifdef _MSC_VER
  std::cout << "_MSC_VER is defined as " <<  _MSC_VER << std::endl;
#else
  std::cout << "_MSC_VER is not defined" << std::endl;
#endif
#ifdef  __CYGWIN__
  std::cout << "__CYGWIN__ is defined" << std::endl;
#else
  std::cout << "__CYGWIN__ is not defined" << std::endl;
#endif
#ifdef __MINGW32__
  std::cout << "__MINGW32__ is defined" << std::endl;
#else
  std::cout << "__MINGW32__ is not defined" << std::endl;
#endif
#ifdef BOOST_POSIX_API
  std::cout << "BOOST_POSIX_API is defined" << std::endl;
#else
  std::cout << "BOOST_POSIX_API is not defined" << std::endl;
#endif
#ifdef BOOST_PLAT_WINDOWS_DESKTOP
  std::cout << "BOOST_PLAT_WINDOWS_DESKTOP is defined as "
    << BOOST_PLAT_WINDOWS_DESKTOP << std::endl;
#else
  std::cout << "BOOST_PLAT_WINDOWS_DESKTOP is not defined" << std::endl;
#endif
#ifdef BOOST_NO_ANSI_APIS
  std::cout << "BOOST_NO_ANSI_APIS is defined" << std::endl;
#else
  std::cout << "BOOST_NO_ANSI_APIS is not defined" << std::endl;
#endif
#ifdef BOOST_NO_CXX11_NOEXCEPT
  std::cout << "BOOST_NO_CXX11_NOEXCEPT is defined" << std::endl;
#else
  std::cout << "BOOST_NO_CXX11_NOEXCEPT is not defined" << std::endl;
#endif
#ifdef BOOST_NO_CXX11_EXPLICIT_CONVERSION_OPERATORS
  std::cout << "BOOST_NO_CXX11_EXPLICIT_CONVERSION_OPERATORS is defined" << std::endl;
#else
  std::cout << "BOOST_NO_CXX11_EXPLICIT_CONVERSION_OPERATORS is not defined" << std::endl;
#endif
  return 0;
}
