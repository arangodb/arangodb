/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library

    Sample: IDL lexer 

    http://www.boost.org/

    Copyright (c) 2001-2010 Hartmut Kaiser. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include <boost/config.hpp>

#if defined(BOOST_HAS_UNISTD_H)
#include <unistd.h>
#else
#include <io.h>
#endif 

#include <boost/assert.hpp>
#include <boost/detail/workaround.hpp>

// reuse the token ids and re2c helper functions from the default C++ lexer
#include <boost/wave/token_ids.hpp>
#include <boost/wave/cpplexer/re2clex/aq.hpp>
#include <boost/wave/cpplexer/re2clex/scanner.hpp>
#include <boost/wave/cpplexer/cpplexer_exceptions.hpp>

#include "idl_re.hpp"

#if defined(_MSC_VER) && !defined(__COMO__)
#pragma warning (disable: 4101)     // 'foo' : unreferenced local variable
#pragma warning (disable: 4102)     // 'foo' : unreferenced label
#endif

#define YYCTYPE   uchar
#define YYCURSOR  cursor
#define YYLIMIT   s->lim
#define YYMARKER  s->ptr
#define YYFILL(n) {cursor = fill(s, cursor);}

//#define BOOST_WAVE_RET(i)    {s->cur = cursor; return (i);}
#define BOOST_WAVE_RET(i)    \
    { \
        s->line += count_backslash_newlines(s, cursor); \
        s->cur = cursor; \
        return (i); \
    } \
    /**/

///////////////////////////////////////////////////////////////////////////////
namespace boost {
namespace wave {
namespace idllexer {
namespace re2clex {

bool is_backslash(
  boost::wave::cpplexer::re2clex::uchar *p, 
  boost::wave::cpplexer::re2clex::uchar *end, int &len)
{
    if (*p == '\\') {
        len = 1;
        return true;
    }
    else if (*p == '?' && *(p+1) == '?' && (p+2 < end && *(p+2) == '/')) {
        len = 3;
        return true;
    }
    return false;
}


///////////////////////////////////////////////////////////////////////////////
}   // namespace re2clex
}   // namespace idllexer
}   // namespace wave
}   // namespace boost
