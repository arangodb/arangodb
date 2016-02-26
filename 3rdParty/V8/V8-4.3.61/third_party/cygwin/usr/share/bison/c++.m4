m4_divert(-1)                                               -*- Autoconf -*-

# C++ skeleton for Bison

# Copyright (C) 2002, 2003, 2004, 2005, 2006 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301  USA

m4_include(b4_pkgdatadir/[c.m4])

## ---------------- ##
## Default values.  ##
## ---------------- ##

# Default parser class name.
m4_define_default([b4_parser_class_name], [parser])
m4_define_default([b4_location_type], [location])
m4_define_default([b4_filename_type], [std::string])
m4_define_default([b4_namespace], m4_defn([b4_prefix]))


# b4_token_enums(LIST-OF-PAIRS-TOKEN-NAME-TOKEN-NUMBER)
# -----------------------------------------------------
# Output the definition of the tokens as enums.
m4_define([b4_token_enums],
[/* Tokens.  */
   enum yytokentype {
m4_map_sep([     b4_token_enum], [,
],
           [$@])
   };
])


## ----------------- ##
## Semantic Values.  ##
## ----------------- ##


# b4_lhs_value([TYPE])
# --------------------
# Expansion of $<TYPE>$.
m4_define([b4_lhs_value],
[(yyval[]m4_ifval([$1], [.$1]))])


# b4_rhs_value(RULE-LENGTH, NUM, [TYPE])
# --------------------------------------
# Expansion of $<TYPE>NUM, where the current rule has RULE-LENGTH
# symbols on RHS.
m4_define([b4_rhs_value],
[(yysemantic_stack_@{($1) - ($2)@}m4_ifval([$3], [.$3]))])

# b4_lhs_location()
# -----------------
# Expansion of @$.
m4_define([b4_lhs_location],
[(yyloc)])


# b4_rhs_location(RULE-LENGTH, NUM)
# ---------------------------------
# Expansion of @NUM, where the current rule has RULE-LENGTH symbols
# on RHS.
m4_define([b4_rhs_location],
[(yylocation_stack_@{($1) - ($2)@})])


# b4_parse_param_decl
# -------------------
# Extra formal arguments of the constructor.
# Change the parameter names from "foo" into "foo_yyarg", so that
# there is no collision bw the user chosen attribute name, and the
# argument name in the constructor.
m4_define([b4_parse_param_decl],
[m4_ifset([b4_parse_param],
          [m4_map_sep([b4_parse_param_decl_1], [, ], [b4_parse_param])])])

m4_define([b4_parse_param_decl_1],
[$1_yyarg])



# b4_parse_param_cons
# -------------------
# Extra initialisations of the constructor.
m4_define([b4_parse_param_cons],
          [m4_ifset([b4_parse_param],
		    [,
      b4_cc_constructor_calls(b4_parse_param)])])
m4_define([b4_cc_constructor_calls],
	  [m4_map_sep([b4_cc_constructor_call], [,
      ], [$@])])
m4_define([b4_cc_constructor_call],
	  [$2 ($2_yyarg)])

# b4_parse_param_vars
# -------------------
# Extra instance variables.
m4_define([b4_parse_param_vars],
          [m4_ifset([b4_parse_param],
		    [
    /* User arguments.  */
b4_cc_var_decls(b4_parse_param)])])
m4_define([b4_cc_var_decls],
	  [m4_map_sep([b4_cc_var_decl], [
], [$@])])
m4_define([b4_cc_var_decl],
	  [    $1;])
