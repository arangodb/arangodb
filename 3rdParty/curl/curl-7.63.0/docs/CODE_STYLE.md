# curl C code style

Source code that has a common style is easier to read than code that uses
different styles in different places. It helps making the code feel like one
single code base. Easy-to-read is a very important property of code and helps
making it easier to review when new things are added and it helps debugging
code when developers are trying to figure out why things go wrong. A unified
style is more important than individual contributors having their own personal
tastes satisfied.

Our C code has a few style rules. Most of them are verified and upheld by the
"lib/checksrc.pl" script. Invoked with "make checksrc" or even by default by
the build system when built after "./configure --enable-debug" has been used.

It is normally not a problem for anyone to follow the guidelines, as you just
need to copy the style already used in the source code and there are no
particularly unusual rules in our set of rules.

We also work hard on writing code that are warning-free on all the major
platforms and in general on as many platforms as possible. Code that obviously
will cause warnings will not be accepted as-is.

## Naming

Try using a non-confusing naming scheme for your new functions and variable
names. It doesn't necessarily have to mean that you should use the same as in
other places of the code, just that the names should be logical,
understandable and be named according to what they're used for. File-local
functions should be made static. We like lower case names.

See the [INTERNALS](INTERNALS.md) document on how we name non-exported
library-global symbols.

## Indenting

We use only spaces for indentation, never TABs. We use two spaces for each new
open brace.

    if(something_is_true) {
      while(second_statement == fine) {
        moo();
      }
    }

## Comments

Since we write C89 code, **//** comments are not allowed. They weren't
introduced in the C standard until C99. We use only **/* comments */**.

    /* this is a comment */

## Long lines

Source code in curl may never be wider than 79 columns and there are two
reasons for maintaining this even in the modern era of very large and high
resolution screens:

1. Narrower columns are easier to read than very wide ones. There's a reason
   newspapers have used columns for decades or centuries.

2. Narrower columns allow developers to easier show multiple pieces of code
   next to each other in different windows. I often have two or three source
   code windows next to each other on the same screen - as well as multiple
   terminal and debugging windows.

## Braces

In if/while/do/for expressions, we write the open brace on the same line as
the keyword and we then set the closing brace on the same indentation level as
the initial keyword. Like this:

    if(age < 40) {
      /* clearly a youngster */
    }

You may omit the braces if they would contain only a one-line statement:

    if(!x)
      continue;

For functions the opening brace should be on a separate line:

    int main(int argc, char **argv)
    {
      return 1;
    }

## 'else' on the following line

When adding an **else** clause to a conditional expression using braces, we
add it on a new line after the closing brace. Like this:

    if(age < 40) {
      /* clearly a youngster */
    }
    else {
      /* probably grumpy */
    }

## No space before parentheses

When writing expressions using if/while/do/for, there shall be no space
between the keyword and the open parenthesis. Like this:

    while(1) {
      /* loop forever */
    }

## Use boolean conditions

Rather than test a conditional value such as a bool against TRUE or FALSE, a
pointer against NULL or != NULL and an int against zero or not zero in
if/while conditions we prefer:

    result = do_something();
    if(!result) {
      /* something went wrong */
      return result;
    }

## No assignments in conditions

To increase readability and reduce complexity of conditionals, we avoid
assigning variables within if/while conditions. We frown upon this style:

    if((ptr = malloc(100)) == NULL)
      return NULL;

and instead we encourage the above version to be spelled out more clearly:

    ptr = malloc(100);
    if(!ptr)
      return NULL;

## New block on a new line

We never write multiple statements on the same source line, even for very
short if() conditions.

    if(a)
      return TRUE;
    else if(b)
      return FALSE;

and NEVER:

    if(a) return TRUE;
    else if(b) return FALSE;

## Space around operators

Please use spaces on both sides of operators in C expressions.  Postfix **(),
[], ->, ., ++, --** and Unary **+, - !, ~, &** operators excluded they should
have no space.

Examples:

    bla = func();
    who = name[0];
    age += 1;
    true = !false;
    size += -2 + 3 * (a + b);
    ptr->member = a++;
    struct.field = b--;
    ptr = &address;
    contents = *pointer;
    complement = ~bits;
    empty = (!*string) ? TRUE : FALSE;

## No parentheses for return values

We use the 'return' statement without extra parentheses around the value:

    int works(void)
    {
      return TRUE;
    }

## Parentheses for sizeof arguments

When using the sizeof operator in code, we prefer it to be written with
parentheses around its argument:

    int size = sizeof(int);

## Column alignment

Some statements cannot be completed on a single line because the line would be
too long, the statement too hard to read, or due to other style guidelines
above. In such a case the statement will span multiple lines.

If a continuation line is part of an expression or sub-expression then you
should align on the appropriate column so that it's easy to tell what part of
the statement it is. Operators should not start continuation lines. In other
cases follow the 2-space indent guideline. Here are some examples from
libcurl:

    if(Curl_pipeline_wanted(handle->multi, CURLPIPE_HTTP1) &&
       (handle->set.httpversion != CURL_HTTP_VERSION_1_0) &&
       (handle->set.httpreq == HTTPREQ_GET ||
        handle->set.httpreq == HTTPREQ_HEAD))
      /* didn't ask for HTTP/1.0 and a GET or HEAD */
      return TRUE;

If no parenthesis, use the default indent:

    data->set.http_disable_hostname_check_before_authentication =
      (0 != va_arg(param, long)) ? TRUE : FALSE;

Function invoke with an open parenthesis:

    if(option) {
      result = parse_login_details(option, strlen(option),
                                   (userp ? &user : NULL),
                                   (passwdp ? &passwd : NULL),
                                   NULL);
    }

Align with the "current open" parenthesis:

    DEBUGF(infof(data, "Curl_pp_readresp_ %d bytes of trailing "
                 "server response left\n",
                 (int)clipamount));

## Platform dependent code

Use **#ifdef HAVE_FEATURE** to do conditional code. We avoid checking for
particular operating systems or hardware in the #ifdef lines. The HAVE_FEATURE
shall be generated by the configure script for unix-like systems and they are
hard-coded in the config-[system].h files for the others.

We also encourage use of macros/functions that possibly are empty or defined
to constants when libcurl is built without that feature, to make the code
seamless. Like this example where the **magic()** function works differently
depending on a build-time conditional:

    #ifdef HAVE_MAGIC
    void magic(int a)
    {
      return a + 2;
    }
    #else
    #define magic(x) 1
    #endif

    int content = magic(3);
