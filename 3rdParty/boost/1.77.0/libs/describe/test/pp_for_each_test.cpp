// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/detail/pp_for_each.hpp>
#include <boost/describe/detail/config.hpp>
#include <boost/core/lightweight_test.hpp>

#if !defined(BOOST_DESCRIBE_CXX11)

#include <boost/config/pragma_message.hpp>

BOOST_PRAGMA_MESSAGE("Skipping test because C++11 is not available")
int main() {}

#else

#define F1(x, y) #y

char const * s00 = "" BOOST_DESCRIBE_PP_FOR_EACH(F1, ~);
char const * s01 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a);
char const * s02 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b);
char const * s03 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c);
char const * s04 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d);
char const * s05 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e);
char const * s06 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f);
char const * s07 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g);
char const * s08 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h);
char const * s09 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i);
char const * s10 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j);
char const * s11 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k);
char const * s12 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l);
char const * s13 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m);
char const * s14 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n);
char const * s15 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o);
char const * s16 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p);
char const * s17 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q);
char const * s18 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r);
char const * s19 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s);
char const * s20 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t);
char const * s21 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u);
char const * s22 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v);
char const * s23 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w);
char const * s24 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x);
char const * s25 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y);
char const * s26 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z);
char const * s27 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A);
char const * s28 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B);
char const * s29 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C);
char const * s30 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D);
char const * s31 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E);
char const * s32 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E, F);
char const * s33 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E, F, G);
char const * s34 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E, F, G, H);
char const * s35 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E, F, G, H, I);
char const * s36 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E, F, G, H, I, J);
char const * s37 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E, F, G, H, I, J, K);
char const * s38 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E, F, G, H, I, J, K, L);
char const * s39 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E, F, G, H, I, J, K, L, M);
char const * s40 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E, F, G, H, I, J, K, L, M, N);
char const * s41 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E, F, G, H, I, J, K, L, M, N, O);
char const * s42 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P);
char const * s43 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q);
char const * s44 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R);
char const * s45 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S);
char const * s46 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T);
char const * s47 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U);
char const * s48 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V);
char const * s49 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W);
char const * s50 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X);
char const * s51 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y);
char const * s52 = BOOST_DESCRIBE_PP_FOR_EACH(F1, ~, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z);

int main()
{
    BOOST_TEST_CSTR_EQ( s00, "" );
    BOOST_TEST_CSTR_EQ( s01, "a" );
    BOOST_TEST_CSTR_EQ( s02, "ab" );
    BOOST_TEST_CSTR_EQ( s03, "abc" );
    BOOST_TEST_CSTR_EQ( s04, "abcd" );
    BOOST_TEST_CSTR_EQ( s05, "abcde" );
    BOOST_TEST_CSTR_EQ( s06, "abcdef" );
    BOOST_TEST_CSTR_EQ( s07, "abcdefg" );
    BOOST_TEST_CSTR_EQ( s08, "abcdefgh" );
    BOOST_TEST_CSTR_EQ( s09, "abcdefghi" );
    BOOST_TEST_CSTR_EQ( s10, "abcdefghij" );
    BOOST_TEST_CSTR_EQ( s11, "abcdefghijk" );
    BOOST_TEST_CSTR_EQ( s12, "abcdefghijkl" );
    BOOST_TEST_CSTR_EQ( s13, "abcdefghijklm" );
    BOOST_TEST_CSTR_EQ( s14, "abcdefghijklmn" );
    BOOST_TEST_CSTR_EQ( s15, "abcdefghijklmno" );
    BOOST_TEST_CSTR_EQ( s16, "abcdefghijklmnop" );
    BOOST_TEST_CSTR_EQ( s17, "abcdefghijklmnopq" );
    BOOST_TEST_CSTR_EQ( s18, "abcdefghijklmnopqr" );
    BOOST_TEST_CSTR_EQ( s19, "abcdefghijklmnopqrs" );
    BOOST_TEST_CSTR_EQ( s20, "abcdefghijklmnopqrst" );
    BOOST_TEST_CSTR_EQ( s21, "abcdefghijklmnopqrstu" );
    BOOST_TEST_CSTR_EQ( s22, "abcdefghijklmnopqrstuv" );
    BOOST_TEST_CSTR_EQ( s23, "abcdefghijklmnopqrstuvw" );
    BOOST_TEST_CSTR_EQ( s24, "abcdefghijklmnopqrstuvwx" );
    BOOST_TEST_CSTR_EQ( s25, "abcdefghijklmnopqrstuvwxy" );
    BOOST_TEST_CSTR_EQ( s26, "abcdefghijklmnopqrstuvwxyz" );
    BOOST_TEST_CSTR_EQ( s27, "abcdefghijklmnopqrstuvwxyzA" );
    BOOST_TEST_CSTR_EQ( s28, "abcdefghijklmnopqrstuvwxyzAB" );
    BOOST_TEST_CSTR_EQ( s29, "abcdefghijklmnopqrstuvwxyzABC" );
    BOOST_TEST_CSTR_EQ( s30, "abcdefghijklmnopqrstuvwxyzABCD" );
    BOOST_TEST_CSTR_EQ( s31, "abcdefghijklmnopqrstuvwxyzABCDE" );
    BOOST_TEST_CSTR_EQ( s32, "abcdefghijklmnopqrstuvwxyzABCDEF" );
    BOOST_TEST_CSTR_EQ( s33, "abcdefghijklmnopqrstuvwxyzABCDEFG" );
    BOOST_TEST_CSTR_EQ( s34, "abcdefghijklmnopqrstuvwxyzABCDEFGH" );
    BOOST_TEST_CSTR_EQ( s35, "abcdefghijklmnopqrstuvwxyzABCDEFGHI" );
    BOOST_TEST_CSTR_EQ( s36, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJ" );
    BOOST_TEST_CSTR_EQ( s37, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJK" );
    BOOST_TEST_CSTR_EQ( s38, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKL" );
    BOOST_TEST_CSTR_EQ( s39, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLM" );
    BOOST_TEST_CSTR_EQ( s40, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMN" );
    BOOST_TEST_CSTR_EQ( s41, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNO" );
    BOOST_TEST_CSTR_EQ( s42, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOP" );
    BOOST_TEST_CSTR_EQ( s43, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQ" );
    BOOST_TEST_CSTR_EQ( s44, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQR" );
    BOOST_TEST_CSTR_EQ( s45, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRS" );
    BOOST_TEST_CSTR_EQ( s46, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRST" );
    BOOST_TEST_CSTR_EQ( s47, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTU" );
    BOOST_TEST_CSTR_EQ( s48, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUV" );
    BOOST_TEST_CSTR_EQ( s49, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVW" );
    BOOST_TEST_CSTR_EQ( s50, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWX" );
    BOOST_TEST_CSTR_EQ( s51, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXY" );
    BOOST_TEST_CSTR_EQ( s52, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ" );

    return boost::report_errors();
}

#endif // !defined(BOOST_DESCRIBE_CXX11)
