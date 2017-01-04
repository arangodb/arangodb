// Copyright (C) 2014 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/predef/platform.h>

#if BOOST_PLAT_WINDOWS_RUNTIME

#include <wrl\wrappers\corewrappers.h>
#pragma comment(lib, "runtimeobject.lib")

// Necessary for the tests which to keep the Windows Runtime active,
// when running in an actual Windows store/phone application initialization
// is handled automatically by the CRT.
// This is easier than calling in the main function for each test case.
Microsoft::WRL::Wrappers::RoInitializeWrapper runtime(RO_INIT_MULTITHREADED);

#endif
