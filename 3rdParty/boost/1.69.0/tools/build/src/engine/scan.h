/*
 * Copyright 1993, 1995 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*
 * scan.h - the jam yacc scanner
 *
 * External functions:
 *  yyerror( char *s ) - print a parsing error message.
 *  yyfparse( char *s ) - scan include file s.
 *  yylex() - parse the next token, returning its type.
 *  yymode() - adjust lexicon of scanner.
 *  yyparse() - declaration for yacc parser.
 *  yyanyerrors() - indicate if any parsing errors occurred.
 *
 * The yymode() function is for the parser to adjust the lexicon of the scanner.
 * Aside from normal keyword scanning, there is a mode to handle action strings
 * (look only for the closing }) and a mode to ignore most keywords when looking
 * for a punctuation keyword. This allows non-punctuation keywords to be used in
 * lists without quoting.
 */

#include "lists.h"
#include "object.h"
#include "parse.h"


/*
 * YYSTYPE - value of a lexical token
 */

#define YYSTYPE YYSYMBOL

typedef struct _YYSTYPE
{
    int          type;
    OBJECT     * string;
    PARSE      * parse;
    LIST       * list;
    int          number;
    OBJECT     * file;
    int          line;
    char const * keyword;
} YYSTYPE;

extern YYSTYPE yylval;

int yymode( int n );
void yyerror( char const * s );
int yyanyerrors();
void yyfparse( OBJECT * s );
void yyfdone( void );
void yysparse( OBJECT * name, const char * * lines );
int yyline();
int yylex();
int yyparse();
void yyinput_last_read_token( OBJECT * * name, int * line );

#define SCAN_NORMAL  0  /* normal parsing */
#define SCAN_STRING  1  /* look only for matching } */
#define SCAN_PUNCT   2  /* only punctuation keywords */
#define SCAN_COND    3  /* look for operators that can appear in conditions. */
#define SCAN_PARAMS  4  /* The parameters of a rule "()*?+" */
#define SCAN_CALL    5  /* Inside a rule call. [].*/
#define SCAN_CASE    6  /* A case statement.  We only recognize ':' as special. */
#define SCAN_CONDB   7  /* The beginning of a condition (ignores leading comparison operators, so that if <x> in $(y) works.)*/
#define SCAN_ASSIGN  8  /* The list may be terminated by an assignment operator. */
#define SCAN_XASSIGN 9  /* The next token might be an assignment, but to token afterwards cannot. */
