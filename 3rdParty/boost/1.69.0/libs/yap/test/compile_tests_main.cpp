// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
void compile_is_expr();
void compile_const_term();
void compile_move_only_types();
void compile_placeholders();
void compile_term_plus_expr();
void compile_term_plus_term();
void compile_term_plus_x();
void compile_term_plus_x_this_ref_overloads();
void compile_x_plus_term();
void compile_user_macros();

int main()
{
    compile_is_expr();
    compile_const_term();
    compile_move_only_types();
    compile_placeholders();
    compile_term_plus_expr();
    compile_term_plus_term();
    compile_term_plus_x();
    compile_term_plus_x_this_ref_overloads();
    compile_x_plus_term();
    compile_user_macros();
}
