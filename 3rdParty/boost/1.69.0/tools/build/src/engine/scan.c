/*
 * Copyright 1993-2002 Christopher Seiwald and Perforce Software, Inc.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*
 * scan.c - the jam yacc scanner
 *
 */

#include "jam.h"
#include "scan.h"
#include "output.h"

#include "constants.h"
#include "jambase.h"
#include "jamgram.h"


struct keyword
{
    char * word;
    int    type;
} keywords[] =
{
#include "jamgramtab.h"
    { 0, 0 }
};

typedef struct include include;
struct include
{
    include   * next;        /* next serial include file */
    char      * string;      /* pointer into current line */
    char    * * strings;     /* for yyfparse() -- text to parse */
    LISTITER    pos;         /* for yysparse() -- text to parse */
    LIST      * list;        /* for yysparse() -- text to parse */
    FILE      * file;        /* for yyfparse() -- file being read */
    OBJECT    * fname;       /* for yyfparse() -- file name */
    int         line;        /* line counter for error messages */
    char        buf[ 512 ];  /* for yyfparse() -- line buffer */
};

static include * incp = 0;  /* current file; head of chain */

static int scanmode = SCAN_NORMAL;
static int anyerrors = 0;


static char * symdump( YYSTYPE * );

#define BIGGEST_TOKEN 10240  /* no single token can be larger */


/*
 * Set parser mode: normal, string, or keyword.
 */

int yymode( int n )
{
    int result = scanmode;
    scanmode = n;
    return result;
}


void yyerror( char const * s )
{
    /* We use yylval instead of incp to access the error location information as
     * the incp pointer will already be reset to 0 in case the error occurred at
     * EOF.
     *
     * The two may differ only if ran into an unexpected EOF or we get an error
     * while reading a lexical token spanning multiple lines, e.g. a multi-line
     * string literal or action body, in which case yylval location information
     * will hold the information about where the token started while incp will
     * hold the information about where reading it broke.
     */
    out_printf( "%s:%d: %s at %s\n", object_str( yylval.file ), yylval.line, s,
            symdump( &yylval ) );
    ++anyerrors;
}


int yyanyerrors()
{
    return anyerrors != 0;
}


void yyfparse( OBJECT * s )
{
    include * i = (include *)BJAM_MALLOC( sizeof( *i ) );

    /* Push this onto the incp chain. */
    i->string = "";
    i->strings = 0;
    i->file = 0;
    i->fname = object_copy( s );
    i->line = 0;
    i->next = incp;
    incp = i;

    /* If the filename is "+", it means use the internal jambase. */
    if ( !strcmp( object_str( s ), "+" ) )
        i->strings = jambase;
}


void yysparse( OBJECT * name, const char * * lines )
{
    yyfparse( name );
    incp->strings = (char * *)lines;
}


/*
 * yyfdone() - cleanup after we're done parsing a file.
 */
void yyfdone( void )
{
    include * const i = incp;
    incp = i->next;

    /* Close file, free name. */
    if(i->file && (i->file != stdin))
        fclose(i->file);
    object_free(i->fname);
    BJAM_FREE((char *)i);
}


/*
 * yyline() - read new line and return first character.
 *
 * Fabricates a continuous stream of characters across include files, returning
 * EOF at the bitter end.
 */

int yyline()
{
    include * const i = incp;

    if ( !incp )
        return EOF;

    /* Once we start reading from the input stream, we reset the include
     * insertion point so that the next include file becomes the head of the
     * list.
     */

    /* If there is more data in this line, return it. */
    if ( *i->string )
        return *i->string++;

    /* If we are reading from an internal string list, go to the next string. */
    if ( i->strings )
    {
        if ( *i->strings )
        {
            ++i->line;
            i->string = *(i->strings++);
            return *i->string++;
        }
    }
    else
    {
        /* If necessary, open the file. */
        if ( !i->file )
        {
            FILE * f = stdin;
            if ( strcmp( object_str( i->fname ), "-" ) && !( f = fopen( object_str( i->fname ), "r" ) ) )
                perror( object_str( i->fname ) );
            i->file = f;
        }

        /* If there is another line in this file, start it. */
        if ( i->file && fgets( i->buf, sizeof( i->buf ), i->file ) )
        {
            ++i->line;
            i->string = i->buf;
            return *i->string++;
        }
    }

    /* This include is done. Return EOF so yyparse() returns to
     * parse_file().
     */

    return EOF;
}

/* This allows us to get an extra character of lookahead.
 * There are a few places where we need to look ahead two
 * characters and yyprev only guarantees a single character
 * of putback.
 */
int yypeek()
{
    if ( *incp->string )
    {
        return *incp->string;
    }
    else if ( incp->strings )
    {
        if ( *incp->strings )
            return **incp->strings;
    }
    else if ( incp->file )
    {
        /* Don't bother opening the file.  yypeek is
         * only used in special cases and never at the
         * beginning of a file.
         */
        int ch = fgetc( incp->file );
        if ( ch != EOF )
            ungetc( ch, incp->file );
        return ch;
    }
    return EOF;
}

/*
 * yylex() - set yylval to current token; return its type.
 *
 * Macros to move things along:
 *
 *  yychar() - return and advance character; invalid after EOF.
 *  yyprev() - back up one character; invalid before yychar().
 *
 * yychar() returns a continuous stream of characters, until it hits the EOF of
 * the current include file.
 */

#define yychar() ( *incp->string ? *incp->string++ : yyline() )
#define yyprev() ( incp->string-- )

static int use_new_scanner = 0;
static int expect_whitespace;

#define yystartkeyword() if(use_new_scanner) break; else token_warning()
#define yyendkeyword() if(use_new_scanner) break; else if ( 1 ) { expect_whitespace = 1; continue; } else (void)0

void do_token_warning()
{
    out_printf( "%s:%d: %s %s\n", object_str( yylval.file ), yylval.line, "Unescaped special character in",
            symdump( &yylval ) );
}

#define token_warning() has_token_warning = 1

int yylex()
{
    int c;
    char buf[ BIGGEST_TOKEN ];
    char * b = buf;

    if ( !incp )
        goto eof;

    /* Get first character (whitespace or of token). */
    c = yychar();

    if ( scanmode == SCAN_STRING )
    {
        /* If scanning for a string (action's {}'s), look for the closing brace.
         * We handle matching braces, if they match.
         */

        int nest = 1;

        while ( ( c != EOF ) && ( b < buf + sizeof( buf ) ) )
        {
            if ( c == '{' )
                ++nest;

            if ( ( c == '}' ) && !--nest )
                break;

            *b++ = c;

            c = yychar();

            /* Turn trailing "\r\n" sequences into plain "\n" for Cygwin. */
            if ( ( c == '\n' ) && ( b[ -1 ] == '\r' ) )
                --b;
        }

        /* We ate the ending brace -- regurgitate it. */
        if ( c != EOF )
            yyprev();

        /* Check for obvious errors. */
        if ( b == buf + sizeof( buf ) )
        {
            yyerror( "action block too big" );
            goto eof;
        }

        if ( nest )
        {
            yyerror( "unmatched {} in action block" );
            goto eof;
        }

        *b = 0;
        yylval.type = STRING;
        yylval.string = object_new( buf );
        yylval.file = incp->fname;
        yylval.line = incp->line;
    }
    else
    {
        char * b = buf;
        struct keyword * k;
        int inquote = 0;
        int notkeyword;
        int hastoken = 0;
        int hasquote = 0;
        int ingrist = 0;
        int invarexpand = 0;
        int expect_whitespace = 0;
        int has_token_warning = 0;

        /* Eat white space. */
        for ( ; ; )
        {
            /* Skip past white space. */
            while ( ( c != EOF ) && isspace( c ) )
                c = yychar();

            /* Not a comment? */
            if ( c != '#' )
                break;

            c = yychar();
            if ( ( c != EOF ) && c == '|' )
            {
                /* Swallow up block comment. */
                int c0 = yychar();
                int c1 = yychar();
                while ( ! ( c0 == '|' && c1 == '#' ) && ( c0 != EOF && c1 != EOF ) )
                {
                    c0 = c1;
                    c1 = yychar();
                }
                c = yychar();
            }
            else
            {
                /* Swallow up comment line. */
                while ( ( c != EOF ) && ( c != '\n' ) ) c = yychar();
            }
        }

        /* c now points to the first character of a token. */
        if ( c == EOF )
            goto eof;

        yylval.file = incp->fname;
        yylval.line = incp->line;

        /* While scanning the word, disqualify it for (expensive) keyword lookup
         * when we can: $anything, "anything", \anything
         */
        notkeyword = c == '$';

        /* Look for white space to delimit word. "'s get stripped but preserve
         * white space. \ protects next character.
         */
        while
        (
            ( c != EOF ) &&
            ( b < buf + sizeof( buf ) ) &&
            ( inquote || invarexpand || !isspace( c ) )
        )
        {
            if ( expect_whitespace || ( isspace( c ) && ! inquote ) )
            {
                token_warning();
                expect_whitespace = 0;
            }
            if ( !inquote && !invarexpand )
            {
                if ( scanmode == SCAN_COND || scanmode == SCAN_CONDB )
                {
                    if ( hastoken && ( c == '=' || c == '<' || c == '>' || c == '!' || c == '(' || c == ')' || c == '&' || c == '|' ) )
                    {
                        /* Don't treat > as special if we started with a grist. */
                        if ( ! ( scanmode == SCAN_CONDB && ingrist == 1 && c == '>' ) )
                        {
                            yystartkeyword();
                        }
                    }
                    else if ( c == '=' || c == '(' || c == ')' )
                    {
                        *b++ = c;
                        c = yychar();
                        yyendkeyword();
                    }
                    else if ( c == '!' || ( scanmode == SCAN_COND && ( c == '<' || c == '>' ) ) )
                    {
                        *b++ = c;
                        if ( ( c = yychar() ) == '=' )
                        {
                            *b++ = c;
                            c = yychar();
                        }
                        yyendkeyword();
                    }
                    else if ( c == '&' || c == '|' )
                    {
                        *b++ = c;
                        if ( yychar() == c )
                        {
                            *b++ = c;
                            c = yychar();
                        }
                        yyendkeyword();
                    }
                }
                else if ( scanmode == SCAN_PARAMS )
                {
                    if ( c == '*' || c == '+' || c == '?' || c == '(' || c == ')' )
                    {
                        if ( !hastoken )
                        {
                            *b++ = c;
                            c = yychar();
                            yyendkeyword();
                        }
                        else
                        {
                            yystartkeyword();
                        }
                    }
                }
                else if ( scanmode == SCAN_XASSIGN && ! hastoken )
                {
                    if ( c == '=' )
                    {
                        *b++ = c;
                        c = yychar();
                        yyendkeyword();
                    }
                    else if ( c == '+' || c == '?' )
                    {
                        if ( yypeek() == '=' )
                        {
                            *b++ = c;
                            *b++ = yychar();
                            c = yychar();
                            yyendkeyword();
                        }
                    }
                }
                else if ( scanmode == SCAN_NORMAL || scanmode == SCAN_ASSIGN )
                {
                    if ( c == '=' )
                    {
                        if ( !hastoken )
                        {
                            *b++ = c;
                            c = yychar();
                            yyendkeyword();
                        }
                        else
                        {
                            yystartkeyword();
                        }
                    }
                    else if ( c == '+' || c == '?' )
                    {
                        if ( yypeek() == '=' )
                        {
                            if ( hastoken )
                            {
                                yystartkeyword();
                            }
                            else
                            {
                                *b++ = c;
                                *b++ = yychar();
                                c = yychar();
                                yyendkeyword();
                            }
                        }
                    }
                }
                if ( scanmode != SCAN_CASE && ( c == ';' || c == '{' || c == '}' ||
                    ( scanmode != SCAN_PARAMS && ( c == '[' || c == ']' ) ) ) )
                {
                    if ( ! hastoken )
                    {
                        *b++ = c;
                        c = yychar();
                        yyendkeyword();
                    }
                    else
                    {
                        yystartkeyword();
                    }
                }
                else if ( c == ':' )
                {
                    if ( ! hastoken )
                    {
                        *b++ = c;
                        c = yychar();
                        yyendkeyword();
                        break;
                    }
                    else if ( hasquote )
                    {
                        /* Special rules for ':' do not apply after we quote anything. */
                        yystartkeyword();
                    }
                    else if ( ingrist == 0 )
                    {
                        int next = yychar();
                        int is_win_path = 0;
                        int is_conditional = 0;
                        if ( next == '\\' )
                        {
                            if( yypeek() == '\\' )
                            {
                                is_win_path = 1;
                            }
                        }
                        else if ( next == '/' )
                        {
                            is_win_path = 1;
                        }
                        yyprev();
                        if ( is_win_path )
                        {
                            /* Accept windows paths iff they are at the start or immediately follow a grist. */
                            if ( b > buf && isalpha( b[ -1 ] ) && ( b == buf + 1 || b[ -2 ] == '>' ) )
                            {
                                is_win_path = 1;
                            }
                            else
                            {
                                is_win_path = 0;
                            }
                        }
                        if ( next == '<' )
                        {
                            /* Accept conditionals only for tokens that start with "<" or "!<" */
                            if ( ( (b > buf) && (buf[ 0 ] == '<') ) ||
                                ( (b > (buf + 1)) && (buf[ 0 ] == '!') && (buf[ 1 ] == '<') ))
                            {
                                is_conditional = 1;
                            }
                        }
                        if ( !is_conditional && !is_win_path )
                        {
                            yystartkeyword();
                        }
                    }
                }
            }
            hastoken = 1;
            if ( c == '"' )
            {
                /* begin or end " */
                inquote = !inquote;
                hasquote = 1;
                notkeyword = 1;
            }
            else if ( c != '\\' )
            {
                if ( !invarexpand && c == '<' )
                {
                    if ( ingrist == 0 ) ingrist = 1;
                    else ingrist = -1;
                }
                else if ( !invarexpand && c == '>' )
                {
                    if ( ingrist == 1 ) ingrist = 0;
                    else ingrist = -1;
                }
                else if ( c == '$' )
                {
                    if ( ( c = yychar() ) == EOF )
                    {
                        *b++ = '$';
                        break;
                    }
                    else if ( c == '(' )
                    {
                        /* inside $(), we only care about quotes */
                        *b++ = '$';
                        c = '(';
                        ++invarexpand;
                    }
                    else
                    {
                        c = '$';
                        yyprev();
                    }
                }
                else if ( c == '@' )
                {
                    if ( ( c = yychar() ) == EOF )
                    {
                        *b++ = '@';
                        break;
                    }
                    else if ( c == '(' )
                    {
                        /* inside @(), we only care about quotes */
                        *b++ = '@';
                        c = '(';
                        ++invarexpand;
                    }
                    else
                    {
                        c = '@';
                        yyprev();
                    }
                }
                else if ( invarexpand && c == '(' )
                {
                    ++invarexpand;
                }
                else if ( invarexpand && c == ')' )
                {
                    --invarexpand;
                }
                /* normal char */
                *b++ = c;
            }
            else if ( ( c = yychar() ) != EOF )
            {
                /* \c */
                if (c == 'n')
                    c = '\n';
                else if (c == 'r')
                    c = '\r';
                else if (c == 't')
                    c = '\t';
                *b++ = c;
                notkeyword = 1;
            }
            else
            {
                /* \EOF */
                break;
            }

            c = yychar();
        }

        /* Automatically switch modes after reading the token. */
        if ( scanmode == SCAN_CONDB )
            scanmode = SCAN_COND;

        /* Check obvious errors. */
        if ( b == buf + sizeof( buf ) )
        {
            yyerror( "string too big" );
            goto eof;
        }

        if ( inquote )
        {
            yyerror( "unmatched \" in string" );
            goto eof;
        }

        /* We looked ahead a character - back up. */
        if ( c != EOF )
            yyprev();

        /* Scan token table. Do not scan if it is obviously not a keyword or if
         * it is an alphabetic when were looking for punctuation.
         */

        *b = 0;
        yylval.type = ARG;

        if ( !notkeyword && !( isalpha( *buf ) && ( scanmode == SCAN_PUNCT || scanmode == SCAN_PARAMS || scanmode == SCAN_ASSIGN ) ) )
            for ( k = keywords; k->word; ++k )
                if ( ( *buf == *k->word ) && !strcmp( k->word, buf ) )
                { 
                    yylval.type = k->type;
                    yylval.keyword = k->word;  /* used by symdump */
                    break;
                }

        if ( yylval.type == ARG )
            yylval.string = object_new( buf );

        if ( scanmode == SCAN_NORMAL && yylval.type == ARG )
            scanmode = SCAN_XASSIGN;

        if ( has_token_warning )
            do_token_warning();
    }

    if ( DEBUG_SCAN )
        out_printf( "scan %s\n", symdump( &yylval ) );

    return yylval.type;

eof:
    /* We do not reset yylval.file & yylval.line here so unexpected EOF error
     * messages would include correct error location information.
     */
    yylval.type = EOF;
    return yylval.type;
}


static char * symdump( YYSTYPE * s )
{
    static char buf[ BIGGEST_TOKEN + 20 ];
    switch ( s->type )
    {
        case EOF   : sprintf( buf, "EOF"                                        ); break;
        case 0     : sprintf( buf, "unknown symbol %s", object_str( s->string ) ); break;
        case ARG   : sprintf( buf, "argument %s"      , object_str( s->string ) ); break;
        case STRING: sprintf( buf, "string \"%s\""    , object_str( s->string ) ); break;
        default    : sprintf( buf, "keyword %s"       , s->keyword              ); break;
    }
    return buf;
}


/*
 * Get information about the current file and line, for those epsilon
 * transitions that produce a parse.
 */

void yyinput_last_read_token( OBJECT * * name, int * line )
{
    /* TODO: Consider whether and when we might want to report where the last
     * read token ended, e.g. EOF errors inside string literals.
     */
    *name = yylval.file;
    *line = yylval.line;
}
