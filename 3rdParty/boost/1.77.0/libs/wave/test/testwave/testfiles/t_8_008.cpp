/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2001-2012 Hartmut Kaiser. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

// Tests predefined macros

//O --c++20
//O -Werror

//R #line 16 "t_8_008.cpp"
__STDC__                    //R 1 
__STDC_VERSION__            //R 199901L 
__cplusplus                 //R 202002L 
__STDC_HOSTED__             //R 0 
__LINE__                    //R 20 
__FILE__                    //R "$P" 
__BASE_FILE__               //R "$F" 
__WAVE_HAS_VARIADICS__      //R 1 
__INCLUDE_LEVEL__           //R 0 
//R #line 53 "test.cpp"
#line 53 "test.cpp"
__LINE__                    //R 53 
__FILE__                    //R "test.cpp" 
__BASE_FILE__               //R "$F" 


//R #line 59 "test.cpp"
__LINE__                    //R 59 
__FILE__                    //R "test.cpp" 
__BASE_FILE__               //R "$F" 

//H 01: <built-in>(1): __STDC__
//H 02: 1
//H 03: 1
//H 01: <built-in>(1): __STDC_VERSION__
//H 02: 199901L
//H 03: 199901L
//H 01: <built-in>(1): __cplusplus
//H 02: 202002L
//H 03: 202002L
//H 01: <built-in>(1): __STDC_HOSTED__
//H 02: 0
//H 03: 0
//H 01: <built-in>(1): __LINE__
//H 02: 20
//H 03: 20
//H 01: <built-in>(1): __FILE__
//H 02: "$P(t_8_008.cpp)"
//H 03: "$P(t_8_008.cpp)"
//H 01: <built-in>(1): __BASE_FILE__
//H 02: "$F"
//H 03: "$F"
//H 01: <built-in>(1): __WAVE_HAS_VARIADICS__
//H 02: 1
//H 03: 1
//H 01: <built-in>(1): __INCLUDE_LEVEL__
//H 02: 0
//H 03: 0
//H 10: t_8_008.cpp(26): #line
//H 17: 53 "test.cpp" (53, "test.cpp")
//H 01: <built-in>(1): __LINE__
//H 02: 53
//H 03: 53
//H 01: <built-in>(1): __FILE__
//H 02: "test.cpp"
//H 03: "test.cpp"
//H 01: <built-in>(1): __BASE_FILE__
//H 02: "$F"
//H 03: "$F"
//H 01: <built-in>(1): __LINE__
//H 02: 59
//H 03: 59
//H 01: <built-in>(1): __FILE__
//H 02: "test.cpp"
//H 03: "test.cpp"
//H 01: <built-in>(1): __BASE_FILE__
//H 02: "$F"
//H 03: "$F"
