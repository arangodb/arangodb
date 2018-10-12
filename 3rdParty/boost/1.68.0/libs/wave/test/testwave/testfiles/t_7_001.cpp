/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2001-2012 Hartmut Kaiser. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

//O --c++11
//O -Werror

//R #line 17 "t_7_001.cpp"
//R R"(de
//R fg
//R h)"
R"(de
fg
h)"

//R #line 22 "t_7_001.cpp"
"abc"   //R "abc" 
R"(abc)"  //R R"(abc)" 

//R #line 28 "t_7_001.cpp"
//R uR"(de fg
//R h)"
uR"(de \
fg
h)"

//R #line 33 "t_7_001.cpp"
u"abc"      //R u"abc" 
U"def"      //R U"def" 
u8"ghi"     //R u8"ghi" 

//R #line 40 "t_7_001.cpp"
//R R"delim("quoted text
//R with newline")delim"
R"delim("quoted text
with newline")delim"

//R #line 45 "t_7_001.cpp"
//R R"de"lim(some text)de"lim"
R"de"lim(some text)de"lim"

//R #line 49 "t_7_001.cpp"
//R no_newline_at_end_of_file
no_newline_at_end_of_file