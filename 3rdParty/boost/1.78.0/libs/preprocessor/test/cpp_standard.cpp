#include <iostream>
#include <iomanip>
#include <string>
#include <boost/preprocessor/config/config.hpp>

static unsigned int indent = 4;
static unsigned int width = 90;

using std::cout;
using std::istream;

void print_separator()
{
   std::cout <<
"\n\n*********************************************************************\n\n";
}

std::string remove_spaces(const std::string ss)
    {
    
    bool inquotes(false);
    bool escape(false);
    char qchar;
    int len(static_cast<int>(ss.length()));
    
    std::string ret;
    
    for (int i = 0; i < len; ++i)
        {
        
        char ch(ss[i]);
        
        if (inquotes)
            {
            if (escape)
                {
                escape = false;
                }
            else if (ch == '\\')
                {
                escape = true;
                }
            else if (ch == qchar)
                {
                inquotes = false;
                }
            ret.push_back(ch);
            }
        else
            {
            if (ch == '\'' || ch == '"')
                {
                inquotes = true;
                qchar = ch;
                ret.push_back(ch);
                }
            else if (ch != ' ')
                {
                ret.push_back(ch);
                }
            }
        }
    
    return ret;
    }

int print_macro(const std::string name,const std::string expected, const std::string expansion)
{
   int bret(0);
   const std::string sg("Success: ");
   const std::string sb("Failure: ");
   for(unsigned i = 0; i < indent; ++i) std::cout.put(' ');
   if (name == expansion)
    {
    if (expected == expansion)
        {
        std::cout << sg;
        std::cout << std::setw(width);
        cout.setf(istream::left, istream::adjustfield);
        std::cout << name;
        std::cout << " [no value]\n";
        }
    else
        {
        std::cout << sb;
        std::cout << std::setw(width);
        cout.setf(istream::left, istream::adjustfield);
        std::cout << name;
        std::cout << " [no value]: ";
        std::cout << " [expected]: ";
        std::cout << expected << "\n";
        bret = 1;
        }
    }
   else
    {
    
    std::string sexpected(remove_spaces(expected));
    std::string sexpansion(remove_spaces(expansion));
    
    if (sexpected == sexpansion)
        {
        std::cout << sg;
        std::cout << std::setw(width);
        cout.setf(istream::left, istream::adjustfield);
        std::cout << name;
        std::cout << expansion << "\n";
        }
    else
        {
        std::cout << sb;
        std::cout << std::setw(width);
        cout.setf(istream::left, istream::adjustfield);
        std::cout << name;
        std::cout << expansion;
        std::cout << " [expected]: ";
        std::cout << expected << "\n";
        bret = 1;
        }
    }
 return bret;
}

#define STRINGIZE(...) # __VA_ARGS__

#define PRINT_MACRO_RESULTS(X,...) print_macro(std::string(# X), std::string(# __VA_ARGS__), std::string(STRINGIZE(X)))
#define PRINT_MACRO(...) std::string(# __VA_ARGS__)
#define PRINT_EXPECTED(...) std::string(# __VA_ARGS__)
#define PRINT_EXPANSION(...) std::string(STRINGIZE(__VA_ARGS__))

#if __cplusplus > 201703L

int print_macros_common_c20()
{

  int bret = 0;
  
#define LPAREN() (
#define G(Q) 42
#define F(R, X, ...) __VA_OPT__(G R X) )

// int x = F(LPAREN(), 0, <:-);

// replaced by 

// int x = 42;

bret += PRINT_MACRO_RESULTS(int x = F(LPAREN(), 0, <:-);,int x = 42;);

#undef LPAREN
#undef G
#undef F

#define F(...) f(0 __VA_OPT__(,) __VA_ARGS__)
#define G(X, ...) f(0, X __VA_OPT__(,) __VA_ARGS__)
#define SDEF(sname, ...) S sname __VA_OPT__(= { __VA_ARGS__ })
#define EMP

// F(a, b, c) 
// F() 
// F(EMP) 

// replaced by 

// f(0, a, b, c)
// f(0)
// f(0)

// G(a, b, c) 
// G(a, ) 
// G(a) 

// replaced by 

// f(0, a, b, c)
// f(0, a)
// f(0, a)

// SDEF(foo); 
// SDEF(bar, 1, 2); 

// replaced by 

// S foo;
// S bar = { 1, 2 };

bret += PRINT_MACRO_RESULTS(F(a, b, c),f(0, a, b, c));
bret += PRINT_MACRO_RESULTS(F(),f(0));
bret += PRINT_MACRO_RESULTS(F(EMP),f(0));
bret += PRINT_MACRO_RESULTS(G(a, b, c),f(0, a, b, c));
bret += PRINT_MACRO_RESULTS(G(a, ),f(0, a));
bret += PRINT_MACRO_RESULTS(G(a),f(0, a));
bret += PRINT_MACRO_RESULTS(SDEF(foo);,S foo;);
bret += PRINT_MACRO_RESULTS(SDEF(bar, 1, 2);,S bar = { 1, 2 };);

#undef F
#undef G
#undef SDEF
#undef EMP

#define H2(X, Y, ...) __VA_OPT__(X ## Y,) __VA_ARGS__
#define H3(X, ...) #__VA_OPT__(X##X X##X)
#define H4(X, ...) __VA_OPT__(a X ## X) ## b
#define H5A(...) __VA_OPT__()/**/__VA_OPT__()
#define H5B(X) a ## X ## b
#define H5C(X) H5B(X)

// H2(a, b, c, d) 

// replaced by 

// ab, c, d

// H3(, 0) 

// replaced by 

// ""

// H4(, 1) 

// replaced by 

// a b

// H5C(H5A()) 

// replaced by 

// ab

bret += PRINT_MACRO_RESULTS(H2(a, b, c, d),ab, c, d);
bret += PRINT_MACRO_RESULTS(H3(, 0),"");
bret += PRINT_MACRO_RESULTS(H4(, 1),a b);
bret += PRINT_MACRO_RESULTS(H5C(H5A()),ab);

#undef H2
#undef H3
#undef H4
#undef H5A
#undef H5B
#undef H5C

  return bret;

}

#endif

int print_macros_common_1()
{

  int bret = 0;

#define x 3
#define f(a) f(x * (a))
#undef x

#define x 2
#define g f
#define z z[0]
#define h g(~
#define m(a) a(w)
#define w 0,1
#define t(a) a

// f(y+1) + f(f(z)) % t(t(g)(0) + t)(1);
// g(x+(3,4)-w) | h 5) & m(f)^m(m);

// results in

// f(2 * (y+1)) + f(2 * (f(2 * (z[0])))) % f(2 * (0)) + t(1);
// f(2 * (2+(3,4)-0,1)) | f(2 * ( ~ 5)) & f(2 * (0,1))^m(0,1);

bret += PRINT_MACRO_RESULTS(f(y+1) + f(f(z)) % t(t(g)(0) + t)(1);,f(2 * (y+1)) + f(2 * (f(2 * (z[0])))) % f(2 * (0)) + t(1););

#define PRINT_INPUT g(x+(3,4)-w) | h 5) & m(f)^m(m);

bret += print_macro
    (
    std::string("g(x+(3,4)-w) | h 5) & m(f)^m(m);"),
    PRINT_EXPECTED(f(2 * (2+(3,4)-0,1)) | f(2 * ( ~ 5)) & f(2 * (0,1))^m(0,1);),
    PRINT_EXPANSION(PRINT_INPUT)
    );
    
#undef PRINT_INPUT

#undef f
#undef x
#undef g
#undef z
#undef h
#undef m
#undef w
#undef t

  return bret;

}

int print_macros_common_4()
{

  int bret = 0;
  
#define str(s) # s
#define xstr(s) str(s)
#define debug(s, t) printf("x" # s "= %d, x" # t "= %s", x ## s, x ## t)
#define INCFILE(n) vers ## n
#define glue(a, b) a ## b
#define xglue(a, b) glue(a, b)
#define HIGHLOW "hello"
#define LOW LOW ", world"

// debug(1, 2);
// fputs(str(strncmp("abc\0d", "abc", ’\4’) // this goes away
// == 0) str(: @\n), s);
// #include xstr(INCFILE(2).h)
// glue(HIGH, LOW);
// xglue(HIGH, LOW)

// results in

// printf("x" "1" "= %d, x" "2" "= %s", x1, x2);
// fputs("strncmp(\"abc\\0d\", \"abc\", ’\\4’) == 0" ": @\n", s);
// #include "vers2.h" (after macro replacement, before file access)
// "hello";
// "hello" ", world"

bret += PRINT_MACRO_RESULTS(debug(1, 2);,printf("x" "1" "= %d, x" "2" "= %s", x1, x2););
bret += print_macro
    (
    std::string("fputs(str(strncmp(\"abc\\0d\", \"abc\", '\\4') /* this goes away */== 0) str(: @\\n), s);"),
    PRINT_EXPECTED(fputs("strncmp(\"abc\\0d\", \"abc\", '\\4') == 0" ": @\n", s);),
    PRINT_EXPANSION(fputs(str(strncmp("abc\0d", "abc", '\4') /* this goes away */== 0) str(: @\n), s);)
    );
bret += PRINT_MACRO_RESULTS(xstr(INCFILE(2).h),"vers2.h");
bret += PRINT_MACRO_RESULTS(glue(HIGH, LOW);,"hello";);

#if __cplusplus <= 199711L

bret += print_macro
    (
    PRINT_MACRO(xglue(HIGH, LOW)),
    std::string("\"hello\" \", world\""),
    PRINT_EXPANSION(xglue(HIGH, LOW))
    );

#else

bret += PRINT_MACRO_RESULTS(xglue(HIGH, LOW),"hello" ", world");
    
#endif

#undef str
#undef xstr
#undef debug
#undef INCFILE
#undef glue
#undef xglue
#undef HIGHLOW
#undef LOW

  return bret;
  
}

int print_macros_common_2()
{

  int bret = 0;

#define hash_hash # ## #
#define mkstr(a) # a
#define in_between(a) mkstr(a)
#define join(c, d) in_between(c hash_hash d)

// char p[] = join(x, y);

// equivalent to

// char p[] = "x ## y";

bret += PRINT_MACRO_RESULTS(char p[] = join(x, y);,char p[] = "x ## y";);

#undef hash_hash
#undef mkstr
#undef in_between
#undef join

  return bret;
  
}

#if __cplusplus > 199711L

int print_macros_common_3()
{

  int bret = 0;

#define p() int
#define q(x) x
#define r(x,y) x ## y
#define str(x) # x

// p() i[q()] = { q(1), r(2,3), r(4,), r(,5), r(,) };
// char c[2][6] = { str(hello), str() };

// results in

// int i[] = { 1, 23, 4, 5, };
// char c[2][6] = { "hello", "" };

bret += print_macro
    (
    PRINT_MACRO(p() i[q()] = { q(1), r(2,3), r(4,), r(,5), r(,) };),
    PRINT_EXPECTED(int i[] = { 1, 23, 4, 5, };),
    PRINT_EXPANSION(p() i[q()] = { q(1), r(2,3), r(4,), r(,5), r(,) };)
    );
bret += print_macro
    (
    PRINT_MACRO(char c[2][6] = { str(hello), str() };),
    PRINT_EXPECTED(char c[2][6] = { "hello", "" };),
    PRINT_EXPANSION(char c[2][6] = { str(hello), str() };)
    );
    
#undef p
#undef q
#undef r
#undef str

bret += print_macros_common_4();

#define t(x,y,z) x ## y ## z

// int j[] = { t(1,2,3), t(,4,5), t(6,,7), t(8,9,), t(10,,), t(,11,), t(,,12), t(,,) };

// results in

// int j[] = { 123, 45, 67, 89, 10, 11, 12, };

bret += print_macro
    (
    PRINT_MACRO(int j[] = { t(1,2,3), t(,4,5), t(6,,7), t(8,9,), t(10,,), t(,11,), t(,,12), t(,,) };),
    PRINT_EXPECTED(int j[] = { 123, 45, 67, 89, 10, 11, 12, };),
    PRINT_EXPANSION(int j[] = { t(1,2,3), t(,4,5), t(6,,7), t(8,9,), t(10,,), t(,11,), t(,,12), t(,,) };)
    );
    
#undef t

#define debug(...) fprintf(stderr, __VA_ARGS__)
#define showlist(...) puts(#__VA_ARGS__)
#define report(test, ...) ((test) ? puts(#test) : printf(__VA_ARGS__))

// debug("Flag");
// debug("X = %d\n", x);
// showlist(The first, second, and third items.);
// report(x>y, "x is %d but y is %d", x, y);

// results in

// fprintf(stderr, "Flag");
// fprintf(stderr, "X = %d\n", x);
// puts("The first, second, and third items.");
// ((x>y) ? puts("x>y") : printf("x is %d but y is %d", x, y));

#define save_stderr stderr
#undef stderr

bret += PRINT_MACRO_RESULTS(debug("Flag");,fprintf(stderr, "Flag"););
bret += PRINT_MACRO_RESULTS(debug("X = %d\n", x);,fprintf(stderr, "X = %d\n", x););

#define stderr save_stderr

bret += PRINT_MACRO_RESULTS(showlist(The first, second, and third items.);,puts("The first, second, and third items."););
bret += PRINT_MACRO_RESULTS(report(x>y, "x is %d but y is %d", x, y);,((x>y) ? puts("x>y") : printf("x is %d but y is %d", x, y)););

#undef debug
#undef showlist
#undef report

  return bret;
  
}

#endif

int print_macros()
{
  int bret = 0;
  
  print_separator();
  
  std::cout << "__cplusplus = " << __cplusplus;
  
  print_separator();
  
#define OBJ_LIKE (1-1)
#define OBJ_LIKE /* white space */ (1-1) /* other */
#define FTN_LIKE(a) ( a )
#define FTN_LIKE( a )( /* note the white space */ a /* other stuff on this line */ )

#if __cplusplus <= 199711L

bret += print_macros_common_2();
bret += print_macros_common_1();
bret += print_macros_common_4();

#elif __cplusplus <= 201703L

bret += print_macros_common_2();
bret += print_macros_common_1();
bret += print_macros_common_3();

#else

bret += print_macros_common_c20();
bret += print_macros_common_2();
bret += print_macros_common_1();
bret += print_macros_common_3();

#endif

  print_separator();
  
  return bret;
}

int main()
{
  return (print_macros());
}
