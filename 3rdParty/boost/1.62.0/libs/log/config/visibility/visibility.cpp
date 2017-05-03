/*
 *          Copyright Andrey Semashev 2007 - 2015.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */

// Guess what, MSVC doesn't ever fail on unknown options, even with /WX. Hence this additional check.
#if !defined(__GNUC__)
#error Visibility option is only supported by gcc and compatible compilers
#endif

int main(int, char*[])
{
    return 0;
}
