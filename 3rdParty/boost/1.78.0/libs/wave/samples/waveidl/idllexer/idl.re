/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library

    Sample: IDL lexer 

    http://www.boost.org/

    Copyright (c) 2001-2010 Hartmut Kaiser. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

// to build idl.inc from this file:
// re2c -b -o idl.inc idl.re

/*!re2c
re2c:indent:string = "    "; 
any                = [\t\v\f\r\n\040-\377];
anyctrl            = [\000-\377];
OctalDigit         = [0-7];
Digit              = [0-9];
HexDigit           = [a-fA-F0-9];
ExponentPart       = [Ee] [+-]? Digit+;
FractionalConstant = (Digit* "." Digit+) | (Digit+ ".");
FloatingSuffix     = [fF][lL]?|[lL][fF]?;
IntegerSuffix      = [uU][lL]?|[lL][uU]?;
FixedPointSuffix   = [dD];
Backslash          = [\\]|"??/";
EscapeSequence     = Backslash ([abfnrtv?'"] | Backslash | "x" HexDigit+ | OctalDigit OctalDigit? OctalDigit?);
HexQuad            = HexDigit HexDigit HexDigit HexDigit;
UniversalChar      = Backslash ("u" HexQuad | "U" HexQuad HexQuad);
Newline            = "\r\n" | "\n" | "\r";
PPSpace            = ([ \t]|("/*"(any\[*]|Newline|("*"+(any\[*/]|Newline)))*"*"+"/"))*;
Pound              = "#" | "??=" | "%:";
*/

/*!re2c
    "/*"            { goto ccomment; }
    "//"            { goto cppcomment; }
    
    "TRUE"          { BOOST_WAVE_RET(T_TRUE); }
    "FALSE"         { BOOST_WAVE_RET(T_FALSE); }
    
    "{"             { BOOST_WAVE_RET(T_LEFTBRACE); }
    "}"             { BOOST_WAVE_RET(T_RIGHTBRACE); }
    "["             { BOOST_WAVE_RET(T_LEFTBRACKET); }
    "]"             { BOOST_WAVE_RET(T_RIGHTBRACKET); }
    "#"             { BOOST_WAVE_RET(T_POUND); }
    "##"            { BOOST_WAVE_RET(T_POUND_POUND); }
    "("             { BOOST_WAVE_RET(T_LEFTPAREN); }
    ")"             { BOOST_WAVE_RET(T_RIGHTPAREN); }
    ";"             { BOOST_WAVE_RET(T_SEMICOLON); }
    ":"             { BOOST_WAVE_RET(T_COLON); }
    "?"             { BOOST_WAVE_RET(T_QUESTION_MARK); }
    "."             { BOOST_WAVE_RET(T_DOT); }
    "+"             { BOOST_WAVE_RET(T_PLUS); }
    "-"             { BOOST_WAVE_RET(T_MINUS); }
    "*"             { BOOST_WAVE_RET(T_STAR); }
    "/"             { BOOST_WAVE_RET(T_DIVIDE); }
    "%"             { BOOST_WAVE_RET(T_PERCENT); }
    "^"             { BOOST_WAVE_RET(T_XOR); }
    "&"             { BOOST_WAVE_RET(T_AND); }
    "|"             { BOOST_WAVE_RET(T_OR); }
    "~"             { BOOST_WAVE_RET(T_COMPL); }
    "!"             { BOOST_WAVE_RET(T_NOT); }
    "="             { BOOST_WAVE_RET(T_ASSIGN); }
    "<"             { BOOST_WAVE_RET(T_LESS); }
    ">"             { BOOST_WAVE_RET(T_GREATER); }
    "<<"            { BOOST_WAVE_RET(T_SHIFTLEFT); }
    ">>"            { BOOST_WAVE_RET(T_SHIFTRIGHT); }
    "=="            { BOOST_WAVE_RET(T_EQUAL); }
    "!="            { BOOST_WAVE_RET(T_NOTEQUAL); }
    "<="            { BOOST_WAVE_RET(T_LESSEQUAL); }
    ">="            { BOOST_WAVE_RET(T_GREATEREQUAL); }
    "&&"            { BOOST_WAVE_RET(T_ANDAND); }
    "||"            { BOOST_WAVE_RET(T_OROR); }
    "++"            { BOOST_WAVE_RET(T_PLUSPLUS); }
    "--"            { BOOST_WAVE_RET(T_MINUSMINUS); }
    ","             { BOOST_WAVE_RET(T_COMMA); }

    ([a-zA-Z_] | UniversalChar) ([a-zA-Z_0-9] | UniversalChar)*        
        { BOOST_WAVE_RET(T_IDENTIFIER); }
    
    (("0" [xX] HexDigit+) | ("0" OctalDigit*) | ([1-9] Digit*)) IntegerSuffix?
        { BOOST_WAVE_RET(T_INTLIT); }

    ((FractionalConstant ExponentPart?) | (Digit+ ExponentPart)) FloatingSuffix?
        { BOOST_WAVE_RET(T_FLOATLIT); }

    (FractionalConstant | Digit+) FixedPointSuffix
        { BOOST_WAVE_RET(T_FIXEDPOINTLIT); }
        
    "L"? (['] (EscapeSequence|any\[\n\r\\']|UniversalChar)+ ['])
        { BOOST_WAVE_RET(T_CHARLIT); }
    
    "L"? (["] (EscapeSequence|any\[\n\r\\"]|UniversalChar)* ["])
        { BOOST_WAVE_RET(T_STRINGLIT); }
    

    Pound PPSpace "include" PPSpace "<" (any\[\n\r>])+ ">" 
        { BOOST_WAVE_RET(T_PP_HHEADER); }

    Pound PPSpace "include" PPSpace "\"" (any\[\n\r"])+ "\"" 
        { BOOST_WAVE_RET(T_PP_QHEADER); } 

    Pound PPSpace "include" PPSpace
        { BOOST_WAVE_RET(T_PP_INCLUDE); } 

    Pound PPSpace "if"        { BOOST_WAVE_RET(T_PP_IF); }
    Pound PPSpace "ifdef"     { BOOST_WAVE_RET(T_PP_IFDEF); }
    Pound PPSpace "ifndef"    { BOOST_WAVE_RET(T_PP_IFNDEF); }
    Pound PPSpace "else"      { BOOST_WAVE_RET(T_PP_ELSE); }
    Pound PPSpace "elif"      { BOOST_WAVE_RET(T_PP_ELIF); }
    Pound PPSpace "endif"     { BOOST_WAVE_RET(T_PP_ENDIF); }
    Pound PPSpace "define"    { BOOST_WAVE_RET(T_PP_DEFINE); }
    Pound PPSpace "undef"     { BOOST_WAVE_RET(T_PP_UNDEF); }
    Pound PPSpace "line"      { BOOST_WAVE_RET(T_PP_LINE); }
    Pound PPSpace "error"     { BOOST_WAVE_RET(T_PP_ERROR); }
    Pound PPSpace "pragma"    { BOOST_WAVE_RET(T_PP_PRAGMA); }

    Pound PPSpace "warning"   { BOOST_WAVE_RET(T_PP_WARNING); }
    
    [ \t\v\f]+
        { BOOST_WAVE_RET(T_SPACE); }

    Newline
    {
        s->line++;
        BOOST_WAVE_RET(T_NEWLINE);
    }

    "\000"
    {
        if(cursor != s->eof) 
        {
            using namespace std;      // some systems have printf in std
            if (0 != s->error_proc) {
                (*s->error_proc)(s, 
                    cpplexer::lexing_exception::generic_lexing_error,
                    "'\\000' in input stream");
            }
            else
                printf("Error: 0 in file\n");
        }
        BOOST_WAVE_RET(T_EOF);
    }

    anyctrl
    {
        BOOST_WAVE_RET(TOKEN_FROM_ID(*s->tok, UnknownTokenType));
    }
*/

ccomment:
/*!re2c
    "*/"            { BOOST_WAVE_RET(T_CCOMMENT); }
    Newline
    {
        /*if(cursor == s->eof) BOOST_WAVE_RET(T_EOF);*/
        /*s->tok = cursor; */
        s->line += count_backslash_newlines(s, cursor) +1;
        goto ccomment;
    }

    any            { goto ccomment; }

    "\000"
    {
        using namespace std;      // some systems have printf in std
        if(cursor == s->eof) 
        {
            if (s->error_proc)
                (*s->error_proc)(s, 
                    cpplexer::lexing_exception::generic_lexing_warning,
                    "Unterminated comment");
            else
                printf("Error: Unterminated comment\n");
        }
        else
        {
            if (s->error_proc)
                (*s->error_proc)(s, 
                    cpplexer::lexing_exception::generic_lexing_error,
                    "'\\000' in input stream");
            else
                printf("Error: 0 in file");
        }
        /* adjust cursor such next call returns T_EOF */
        --YYCURSOR;
        /* the comment is unterminated, but nevertheless its a comment */
        BOOST_WAVE_RET(T_CCOMMENT);
    }

    anyctrl
    {
        if (s->error_proc)
            (*s->error_proc)(s, 
                cpplexer::lexing_exception::generic_lexing_error,
                "invalid character in input stream");
        else
            printf("Error: 0 in file");
    }

*/

cppcomment:
/*!re2c
    Newline
    {
        /*if(cursor == s->eof) BOOST_WAVE_RET(T_EOF); */
        /*s->tok = cursor; */
        s->line++;
        BOOST_WAVE_RET(T_CPPCOMMENT);
    }

    any            { goto cppcomment; }

    "\000"
    {
        using namespace std;      // some systems have printf in std
        if(cursor != s->eof) 
        {
            if (s->error_proc)
                (*s->error_proc)(s, 
                    cpplexer::lexing_exception::generic_lexing_error,
                    "'\\000' in input stream");
            else
                printf("Error: 0 in file");
        }
        /* adjust cursor such next call returns T_EOF */
        --YYCURSOR;
        /* the comment is unterminated, but nevertheless its a comment */
        BOOST_WAVE_RET(T_CPPCOMMENT);
    }
*/
