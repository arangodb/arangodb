m4_divert(-1)                                               -*- Autoconf -*-

# C M4 Macros for Bison.
# Copyright (C) 2002, 2004, 2005, 2006 Free Software Foundation, Inc.

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


## ---------------- ##
## Identification.  ##
## ---------------- ##

# b4_copyright(TITLE, YEARS)
# --------------------------
m4_define([b4_copyright],
[/* A Bison parser, made by GNU Bison b4_version.  */

/* $1

m4_text_wrap([Copyright (C) $2 Free Software Foundation, Inc.], [   ])

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */])


# b4_identification
# -----------------
m4_define([b4_identification],
[/* Identify Bison output.  */
[#]define YYBISON 1

/* Bison version.  */
[#]define YYBISON_VERSION "b4_version"

/* Skeleton name.  */
[#]define YYSKELETON_NAME b4_skeleton

/* Pure parsers.  */
[#]define YYPURE b4_pure_flag

/* Using locations.  */
[#]define YYLSP_NEEDED b4_locations_flag
])



## ---------------- ##
## Default values.  ##
## ---------------- ##

m4_define_default([b4_epilogue], [])



## ------------------------ ##
## Pure/impure interfaces.  ##
## ------------------------ ##


# b4_user_args
# ------------
m4_define([b4_user_args],
[m4_ifset([b4_parse_param], [, b4_c_args(b4_parse_param)])])


# b4_parse_param
# --------------
# If defined, b4_parse_param arrives double quoted, but below we prefer
# it to be single quoted.
m4_define_default([b4_parse_param])
m4_define([b4_parse_param],
b4_parse_param))


# b4_parse_param_for(DECL, FORMAL, BODY)
# ---------------------------------------
# Iterate over the user parameters, binding the declaration to DECL,
# the formal name to FORMAL, and evaluating the BODY.
m4_define([b4_parse_param_for],
[m4_foreach([$1_$2], m4_defn([b4_parse_param]),
[m4_pushdef([$1], m4_fst($1_$2))dnl
m4_pushdef([$2], m4_shift($1_$2))dnl
$3[]dnl
m4_popdef([$2])dnl
m4_popdef([$1])dnl
])])

# b4_parse_param_use
# ------------------
# `YYUSE' all the parse-params.
m4_define([b4_parse_param_use],
[b4_parse_param_for([Decl], [Formal], [  YYUSE (Formal);
])dnl
])

## ------------ ##
## Data Types.  ##
## ------------ ##


# b4_ints_in(INT1, INT2, LOW, HIGH)
# ---------------------------------
# Return 1 iff both INT1 and INT2 are in [LOW, HIGH], 0 otherwise.
m4_define([b4_ints_in],
[m4_eval([$3 <= $1 && $1 <= $4 && $3 <= $2 && $2 <= $4])])


# b4_int_type(MIN, MAX)
# ---------------------
# Return the smallest int type able to handle numbers ranging from
# MIN to MAX (included).
m4_define([b4_int_type],
[m4_if(b4_ints_in($@,      [0],   [255]), [1], [unsigned char],
       b4_ints_in($@,   [-128],   [127]), [1], [signed char],

       b4_ints_in($@,      [0], [65535]), [1], [unsigned short int],
       b4_ints_in($@, [-32768], [32767]), [1], [short int],

       m4_eval([0 <= $1]),                [1], [unsigned int],

					       [int])])


# b4_int_type_for(NAME)
# ---------------------
# Return the smallest int type able to handle numbers ranging from
# `NAME_min' to `NAME_max' (included).
m4_define([b4_int_type_for],
[b4_int_type($1_min, $1_max)])


## ------------------ ##
## Decoding options.  ##
## ------------------ ##

# b4_flag_if(FLAG, IF-TRUE, IF-FALSE)
# -----------------------------------
# Run IF-TRUE if b4_FLAG_flag is 1, IF-FALSE if FLAG is 0, otherwise fail.
m4_define([b4_flag_if],
[m4_case(b4_$1_flag,
         [0], [$3],
	 [1], [$2],
	 [m4_fatal([invalid $1 value: ]$1)])])


# b4_define_flag_if(FLAG)
# -----------------------
# Define "b4_FLAG_if(IF-TRUE, IF-FALSE)" that depends on the
# value of the Boolean FLAG.
m4_define([b4_define_flag_if],
[_b4_define_flag_if($[1], $[2], [$1])])

# _b4_define_flag_if($1, $2, FLAG)
# --------------------------------
# This macro works around the impossibility to define macros
# inside macros, because issuing `[$1]' is not possible in M4 :(.
# This sucks hard, GNU M4 should really provide M5 like $$1.
m4_define([_b4_define_flag_if],
[m4_if([$1$2], $[1]$[2], [],
       [m4_fatal([$0: Invalid arguments: $@])])dnl
m4_define([b4_$3_if], 
          [b4_flag_if([$3], [$1], [$2])])])


# b4_FLAG_if(IF-TRUE, IF-FALSE)
# -----------------------------
# Expand IF-TRUE, if FLAG is true, IF-FALSE otherwise.
b4_define_flag_if([defines])        # Whether headers are requested.
b4_define_flag_if([error_verbose])  # Wheter error are verbose.
b4_define_flag_if([locations])      # Whether locations are tracked.
b4_define_flag_if([pure])           # Whether the interface is pure.



## ------------------------- ##
## Assigning token numbers.  ##
## ------------------------- ##

# b4_token_define(TOKEN-NAME, TOKEN-NUMBER)
# -----------------------------------------
# Output the definition of this token as #define.
m4_define([b4_token_define],
[#define $1 $2
])


# b4_token_defines(LIST-OF-PAIRS-TOKEN-NAME-TOKEN-NUMBER)
# -------------------------------------------------------
# Output the definition of the tokens (if there are) as #defines.
m4_define([b4_token_defines],
[m4_if([$@], [[]], [],
[/* Tokens.  */
m4_map([b4_token_define], [$@])])
])


# b4_token_enum(TOKEN-NAME, TOKEN-NUMBER)
# ---------------------------------------
# Output the definition of this token as an enum.
m4_define([b4_token_enum],
[$1 = $2])


# b4_token_enums(LIST-OF-PAIRS-TOKEN-NAME-TOKEN-NUMBER)
# -----------------------------------------------------
# Output the definition of the tokens (if there are) as enums.
m4_define([b4_token_enums],
[m4_if([$@], [[]], [],
[/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
m4_map_sep([     b4_token_enum], [,
],
	   [$@])
   };
#endif
])])


# b4_token_enums_defines(LIST-OF-PAIRS-TOKEN-NAME-TOKEN-NUMBER)
# -------------------------------------------------------------
# Output the definition of the tokens (if there are) as enums and #defines.
m4_define([b4_token_enums_defines],
[b4_token_enums($@)b4_token_defines($@)
])



## --------------------------------------------- ##
## Defining C functions in both K&R and ANSI-C.  ##
## --------------------------------------------- ##


# b4_modern_c
# -----------
# A predicate useful in #if to determine whether C is ancient or modern.
#
# If __STDC__ is defined, the compiler is modern.  IBM xlc 7.0 when run
# as 'cc' doesn't define __STDC__ (or __STDC_VERSION__) for pedantic
# reasons, but it defines __C99__FUNC__ so check that as well.
# Microsoft C normally doesn't define these macros, but it defines _MSC_VER.
# Consider a C++ compiler to be modern if it defines __cplusplus.
#
m4_define([b4_c_modern],
  [[(defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)]])

# b4_c_function_def(NAME, RETURN-VALUE, [DECL1, NAME1], ...)
# ----------------------------------------------------------
# Declare the function NAME.
m4_define([b4_c_function_def],
[#if b4_c_modern
b4_c_ansi_function_def($@)
#else
$2
$1 (b4_c_knr_formal_names(m4_shiftn(2, $@)))
b4_c_knr_formal_decls(m4_shiftn(2, $@))
#endif[]dnl
])


# b4_c_ansi_function_def(NAME, RETURN-VALUE, [DECL1, NAME1], ...)
# ---------------------------------------------------------------
# Declare the function NAME in ANSI.
m4_define([b4_c_ansi_function_def],
[$2
$1 (b4_c_ansi_formals(m4_shiftn(2, $@)))[]dnl
])


# b4_c_ansi_formals([DECL1, NAME1], ...)
# --------------------------------------
# Output the arguments ANSI-C definition.
m4_define([b4_c_ansi_formals],
[m4_case([$@],
	 [],   [void],
	 [[]], [void],
	       [m4_map_sep([b4_c_ansi_formal], [, ], [$@])])])

m4_define([b4_c_ansi_formal],
[$1])


# b4_c_knr_formal_names([DECL1, NAME1], ...)
# ------------------------------------------
# Output the argument names.
m4_define([b4_c_knr_formal_names],
[m4_map_sep([b4_c_knr_formal_name], [, ], [$@])])

m4_define([b4_c_knr_formal_name],
[$2])


# b4_c_knr_formal_decls([DECL1, NAME1], ...)
# ------------------------------------------
# Output the K&R argument declarations.
m4_define([b4_c_knr_formal_decls],
[m4_map_sep([b4_c_knr_formal_decl],
	    [
],
	    [$@])])

m4_define([b4_c_knr_formal_decl],
[    $1;])



## ------------------------------------------------------------ ##
## Declaring (prototyping) C functions in both K&R and ANSI-C.  ##
## ------------------------------------------------------------ ##


# b4_c_function_decl(NAME, RETURN-VALUE, [DECL1, NAME1], ...)
# -----------------------------------------------------------
# Declare the function NAME.
m4_define([b4_c_function_decl],
[#if defined __STDC__ || defined __cplusplus
b4_c_ansi_function_decl($@)
#else
$2 $1 ();
#endif[]dnl
])


# b4_c_ansi_function_decl(NAME, RETURN-VALUE, [DECL1, NAME1], ...)
# ----------------------------------------------------------------
# Declare the function NAME.
m4_define([b4_c_ansi_function_decl],
[$2 $1 (b4_c_ansi_formals(m4_shiftn(2, $@)));[]dnl
])




## --------------------- ##
## Calling C functions.  ##
## --------------------- ##


# b4_c_function_call(NAME, RETURN-VALUE, [DECL1, NAME1], ...)
# -----------------------------------------------------------
# Call the function NAME with arguments NAME1, NAME2 etc.
m4_define([b4_c_function_call],
[$1 (b4_c_args(m4_shiftn(2, $@)))[]dnl
])


# b4_c_args([DECL1, NAME1], ...)
# ------------------------------
# Output the arguments NAME1, NAME2...
m4_define([b4_c_args],
[m4_map_sep([b4_c_arg], [, ], [$@])])

m4_define([b4_c_arg],
[$2])


## ----------- ##
## Synclines.  ##
## ----------- ##

# b4_syncline(LINE, FILE)
# -----------------------
m4_define([b4_syncline],
[b4_flag_if([synclines], [[#]line $1 $2])])



## -------------- ##
## User actions.  ##
## -------------- ##

# b4_symbol_actions(FILENAME, LINENO,
#                   SYMBOL-TAG, SYMBOL-NUM,
#                   SYMBOL-ACTION, SYMBOL-TYPENAME)
# -------------------------------------------------
m4_define([b4_symbol_actions],
[m4_pushdef([b4_dollar_dollar],
   [m4_ifval([$6], [(yyvaluep->$6)], [(*yyvaluep)])])dnl
m4_pushdef([b4_at_dollar], [(*yylocationp)])dnl
      case $4: /* $3 */
b4_syncline([$2], [$1])
	$5;
b4_syncline([@oline@], [@ofile@])
	break;
m4_popdef([b4_at_dollar])dnl
m4_popdef([b4_dollar_dollar])dnl
])


# b4_yydestruct_generate(FUNCTION-DECLARATOR)
# -------------------------------------------
# Generate the "yydestruct" function, which declaration is issued using
# FUNCTION-DECLARATOR, which may be "b4_c_ansi_function_def" for ISO C
# or "b4_c_function_def" for K&R.
m4_define_default([b4_yydestruct_generate],
[[/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
]$1([yydestruct],
    [static void],
    [[const char *yymsg],    [yymsg]],
    [[int yytype],           [yytype]],
    [[YYSTYPE *yyvaluep],    [yyvaluep]][]dnl
b4_locations_if(            [, [[YYLTYPE *yylocationp], [yylocationp]]])[]dnl
m4_ifset([b4_parse_param], [, b4_parse_param]))[
{
  YYUSE (yyvaluep);
]b4_locations_if([  YYUSE (yylocationp);
])dnl
b4_parse_param_use[]dnl
[
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {
]m4_map([b4_symbol_actions], m4_defn([b4_symbol_destructors]))[
      default:
	break;
    }
}]dnl
])


# b4_yy_symbol_print_generate(FUNCTION-DECLARATOR)
# ------------------------------------------------
# Generate the "yy_symbol_print" function, which declaration is issued using
# FUNCTION-DECLARATOR, which may be "b4_c_ansi_function_def" for ISO C
# or "b4_c_function_def" for K&R.
m4_define_default([b4_yy_symbol_print_generate],
[[
/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
]$1([yy_symbol_value_print],
    [static void],
	       [[FILE *yyoutput],                       [yyoutput]],
	       [[int yytype],                           [yytype]],
	       [[YYSTYPE const * const yyvaluep],       [yyvaluep]][]dnl
b4_locations_if([, [[YYLTYPE const * const yylocationp], [yylocationp]]])[]dnl
m4_ifset([b4_parse_param], [, b4_parse_param]))[
{
  if (!yyvaluep)
    return;
]b4_locations_if([  YYUSE (yylocationp);
])dnl
b4_parse_param_use[]dnl
[# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
]m4_map([b4_symbol_actions], m4_defn([b4_symbol_printers]))dnl
[      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

]$1([yy_symbol_print],
    [static void],
	       [[FILE *yyoutput],                       [yyoutput]],
	       [[int yytype],                           [yytype]],
	       [[YYSTYPE const * const yyvaluep],       [yyvaluep]][]dnl
b4_locations_if([, [[YYLTYPE const * const yylocationp], [yylocationp]]])[]dnl
m4_ifset([b4_parse_param], [, b4_parse_param]))[
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

]b4_locations_if([  YY_LOCATION_PRINT (yyoutput, *yylocationp);
  YYFPRINTF (yyoutput, ": ");
])dnl
[  yy_symbol_value_print (yyoutput, yytype, yyvaluep]dnl
b4_locations_if([, yylocationp])[]b4_user_args[);
  YYFPRINTF (yyoutput, ")");
}]dnl
])
