/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2020 Jeff Trull. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

//O --c++20
//O -Werror

// __VA_OPT__ examples from the proposal doc P00306
// Those examples have a few weird typos. I've tried to match the
// behavior of gcc in the expected results - JET

#define LOG(msg, ...) printf(msg __VA_OPT__(,) __VA_ARGS__)
#define SDEF(sname, ...) S sname __VA_OPT__(= { __VA_ARGS__ })
#define LOG2(...)                                     \
    printf("at line=%d" __VA_OPT__(": "), __LINE__);  \
    __VA_OPT__(printf(__VA_ARGS__);)                  \
    printf("\n")

//R #line 28 "t_8_001.cpp"
//R printf("hello world\n" );
//R printf("hello world\n" );
//R printf("hello %d\n" , n);
LOG("hello world\n");
LOG("hello world\n", );
LOG("hello %d\n", n);

//R #line 35 "t_8_001.cpp"
//R S foo;
//R S bar = { 1, 2, 3 };
SDEF(foo);
SDEF(bar, 1, 2, 3);

//R #line 43 "t_8_001.cpp"
//R printf("at line=%d" , 43); printf("\n");
// P00306 example suggests the adjacent string literals are concatenated in
// this test case but that's the *compiler's* job:
//R printf("at line=%d" ": ", 44); printf("All well in zone %d", n); printf("\n");
LOG2();
LOG2("All well in zone %d", n);

//H 10: t_8_001.cpp(17): #define
//H 08: t_8_001.cpp(17): LOG(msg, ...)=printf(msg __VA_OPT__(,) __VA_ARGS__)
//H 10: t_8_001.cpp(18): #define
//H 08: t_8_001.cpp(18): SDEF(sname, ...)=S sname __VA_OPT__(= { __VA_ARGS__ })
//H 10: t_8_001.cpp(19): #define
//H 08: t_8_001.cpp(19): LOG2(...)=printf("at line=%d" __VA_OPT__(": "), __LINE__);      __VA_OPT__(printf(__VA_ARGS__);)                      printf("\n")
//H 00: t_8_001.cpp(28): LOG("hello world\n"), [t_8_001.cpp(17): LOG(msg, ...)=printf(msg __VA_OPT__(,) __VA_ARGS__)]
//H 00: t_8_001.cpp(17): __VA_OPT__(,), [<built-in>(1): __VA_OPT__(...)=]
// no variadic args supplied, placemarker produced:
//H 02: §
//H 02: printf("hello world\n" § )
//H 03: printf("hello world\n"  )
//H 00: t_8_001.cpp(29): LOG("hello world\n", §), [t_8_001.cpp(17): LOG(msg, ...)=printf(msg __VA_OPT__(,) __VA_ARGS__)]
//H 00: t_8_001.cpp(17): __VA_OPT__(,), [<built-in>(1): __VA_OPT__(...)=]
// a variadic arg, but it contains a placemarker
// it's a kind of third case - no visible tokens, but not exclusively "whitespace"
//H 02: 
//H 02: printf("hello world\n"  )
//H 03: printf("hello world\n"  )
//H 00: t_8_001.cpp(30): LOG("hello %d\n", n), [t_8_001.cpp(17): LOG(msg, ...)=printf(msg __VA_OPT__(,) __VA_ARGS__)]
//H 00: t_8_001.cpp(17): __VA_OPT__(,), [<built-in>(1): __VA_OPT__(...)=]
// a single token for variadic args:
//H 02: ,
//H 02: printf("hello %d\n" ,  n)
//H 03: printf("hello %d\n" ,  n)
//H 00: t_8_001.cpp(35): SDEF(foo), [t_8_001.cpp(18): SDEF(sname, ...)=S sname __VA_OPT__(= { __VA_ARGS__ })]
//H 00: t_8_001.cpp(18): __VA_OPT__(= { __VA_ARGS__ }), [<built-in>(1): __VA_OPT__(...)=]
// no varargs, emit placeholder:
//H 02: §
//H 02: S foo §
//H 03: S foo
//H 00: t_8_001.cpp(36): SDEF(bar, 1, 2, 3), [t_8_001.cpp(18): SDEF(sname, ...)=S sname __VA_OPT__(= { __VA_ARGS__ })]
//H 00: t_8_001.cpp(18): __VA_OPT__(= { __VA_ARGS__ }), [<built-in>(1): __VA_OPT__(...)=]
//H 02: = {  1, 2, 3 }
//H 02: S bar = {  1, 2, 3 }
//H 03: S bar = {  1, 2, 3 }
//H 00: t_8_001.cpp(43): LOG2(§), [t_8_001.cpp(19): LOG2(...)=printf("at line=%d" __VA_OPT__(": "), __LINE__);      __VA_OPT__(printf(__VA_ARGS__);)                      printf("\n")]
//H 00: t_8_001.cpp(20): __VA_OPT__(": "), [<built-in>(1): __VA_OPT__(...)=]
//H 02: 
//H 00: t_8_001.cpp(21): __VA_OPT__(printf(__VA_ARGS__);), [<built-in>(1): __VA_OPT__(...)=]
//H 02: 
//H 02: printf("at line=%d" , __LINE__);                            printf("\n")
//H 01: <built-in>(1): __LINE__
//H 02: 43
//H 03: 43
//H 03: printf("at line=%d" , 43);                            printf("\n")
//H 00: t_8_001.cpp(44): LOG2("All well in zone %d", n), [t_8_001.cpp(19): LOG2(...)=printf("at line=%d" __VA_OPT__(": "), __LINE__);      __VA_OPT__(printf(__VA_ARGS__);)                      printf("\n")]
//H 00: t_8_001.cpp(20): __VA_OPT__(": "), [<built-in>(1): __VA_OPT__(...)=]
//H 02: ": "
//H 00: t_8_001.cpp(21): __VA_OPT__(printf(__VA_ARGS__);), [<built-in>(1): __VA_OPT__(...)=]
//H 02: printf("All well in zone %d", n);
//H 02: printf("at line=%d" ": ", __LINE__);      printf("All well in zone %d", n);                      printf("\n")
//H 01: <built-in>(1): __LINE__
//H 02: 44
//H 03: 44
//H 03: printf("at line=%d" ": ", 44);      printf("All well in zone %d", n);                      printf("\n")
