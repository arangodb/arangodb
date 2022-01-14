/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2020 Jeff Trull. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    The body of this test is taken directly from a standard proposal:
    http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1911.htm
=============================================================================*/


#line 500
  #define MAC(a,b) a,b,__LINE__

  #line 1000
int j[] = {__LINE__, __\
LINE\
__,

#line 2000
#line __\
LINE\
__
    __LINE__,

#line 3000
#line                                                                          \
                                                                               \
                                                                               \
    __LINE__


    /**/
    __LINE__,

    MAC(__LINE__, __LINE__),

    MAC(__LINE__, __LINE__), __LINE__,

    M\
A\
C

    (

        __\
LINE\
__,
        __LINE__

        ),
    __LINE__,

    M\
A\
C(__\
LINE\
__,
        __LINE__),
    __LINE__, 999};

//R #line 1000 "t_5_037.cpp"
//R int j[] = {1000, 1000,
//R #line 2000 "t_5_037.cpp"
//R 2000,
//R #line 3006 "t_5_037.cpp"
//R 3006,
//R 
//R 3008, 3008,3008,
//R 
//R 3010, 3010,3010, 3010,
//R 
//R 3018, 3021 ,3012,
//R 3024,
//R #line 3026 "t_5_037.cpp"
//R 3028, 3031,3026,
//R 3032, 999};
