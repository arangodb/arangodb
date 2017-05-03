Windows
-------

Prerequisites

Boost libraries available in boost-root\stage\lib. Example:
 
  cd boost-root
  .\bootstrap
  .\b2 --with-system --with-chrono --with-timer link=shared stage

The provided Visual Studio solution (endian/test/msvc/endian.sln) has a property page
(endian/test/msvc/common.prop) with these Common Properties set (do not include the
double quotes):

    VC++ Directores|Executable Directories: prefix default value with "..\..\..\..\..\stage\lib;"
    (Click "Inherit from parent or project defaults" if not checked)

    C/C++|General|Additional Include Directories: prefix default value with "..\..\..\..\..\stage\lib;"
 
    Linker|General|Additional Library Directories: prefix default value with "..\..\..\..\..\stage\lib;"

    C/C++|Preprocessor: prefix default value with "BOOST_ALL_DYN_LINK;"

IMPORTANT: If Preprocessor macros are supplied via a common property page,
<inherit from parent or project defaults> must be set for each project!

------------------------------------------------------------------------------------------
Copyright Beman Dawes, 2013
Distributed under the Boost Software License, Version 1.0.
See http://www.boost.org/LICENSE_1_0.txt
