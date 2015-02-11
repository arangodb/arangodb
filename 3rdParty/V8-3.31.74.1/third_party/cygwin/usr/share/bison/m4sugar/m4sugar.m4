divert(-1)#                                                  -*- Autoconf -*-
# This file is part of Autoconf.
# Base M4 layer.
# Requires GNU M4.
#
# Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005 Free Software
# Foundation, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#
# As a special exception, the Free Software Foundation gives unlimited
# permission to copy, distribute and modify the configure scripts that
# are the output of Autoconf.  You need not follow the terms of the GNU
# General Public License when using or distributing such scripts, even
# though portions of the text of Autoconf appear in them.  The GNU
# General Public License (GPL) does govern all other use of the material
# that constitutes the Autoconf program.
#
# Certain portions of the Autoconf source text are designed to be copied
# (in certain cases, depending on the input) into the output of
# Autoconf.  We call these the "data" portions.  The rest of the Autoconf
# source text consists of comments plus executable code that decides which
# of the data portions to output in any given case.  We call these
# comments and executable code the "non-data" portions.  Autoconf never
# copies any of the non-data portions into its output.
#
# This special exception to the GPL applies to versions of Autoconf
# released by the Free Software Foundation.  When you make and
# distribute a modified version of Autoconf, you may extend this special
# exception to the GPL to apply to your modified version as well, *unless*
# your modified version has the potential to copy into its output some
# of the text that was the non-data portion of the version that you started
# with.  (In other words, unless your change moves or copies text from
# the non-data portions to the data portions.)  If your modification has
# such potential, you must delete any notice of this special exception
# to the GPL from your modified version.
#
# Written by Akim Demaille.
#

# Set the quotes, whatever the current quoting system.
changequote()
changequote([, ])

# Some old m4's don't support m4exit.  But they provide
# equivalent functionality by core dumping because of the
# long macros we define.
ifdef([__gnu__], ,
[errprint(M4sugar requires GNU M4. Install it before installing M4sugar or
set the M4 environment variable to its absolute file name.)
m4exit(2)])


## ------------------------------- ##
## 1. Simulate --prefix-builtins.  ##
## ------------------------------- ##

# m4_define
# m4_defn
# m4_undefine
define([m4_define],   defn([define]))
define([m4_defn],     defn([defn]))
define([m4_undefine], defn([undefine]))

m4_undefine([define])
m4_undefine([defn])
m4_undefine([undefine])


# m4_copy(SRC, DST)
# -----------------
# Define DST as the definition of SRC.
# What's the difference between:
# 1. m4_copy([from], [to])
# 2. m4_define([to], [from($@)])
# Well, obviously 1 is more expensive in space.  Maybe 2 is more expensive
# in time, but because of the space cost of 1, it's not that obvious.
# Nevertheless, one huge difference is the handling of `$0'.  If `from'
# uses `$0', then with 1, `to''s `$0' is `to', while it is `from' in 2.
# The user will certainly prefer to see `to'.
m4_define([m4_copy],
[m4_define([$2], m4_defn([$1]))])


# m4_rename(SRC, DST)
# -------------------
# Rename the macro SRC as DST.
m4_define([m4_rename],
[m4_copy([$1], [$2])m4_undefine([$1])])


# m4_rename_m4(MACRO-NAME)
# ------------------------
# Rename MACRO-NAME as m4_MACRO-NAME.
m4_define([m4_rename_m4],
[m4_rename([$1], [m4_$1])])


# m4_copy_unm4(m4_MACRO-NAME)
# ---------------------------
# Copy m4_MACRO-NAME as MACRO-NAME.
m4_define([m4_copy_unm4],
[m4_copy([$1], m4_bpatsubst([$1], [^m4_\(.*\)], [[\1]]))])


# Some m4 internals have names colliding with tokens we might use.
# Rename them a` la `m4 --prefix-builtins'.
m4_rename_m4([builtin])
m4_rename_m4([changecom])
m4_rename_m4([changequote])
m4_rename_m4([debugfile])
m4_rename_m4([debugmode])
m4_rename_m4([decr])
m4_undefine([divert])
m4_rename_m4([divnum])
m4_rename_m4([dumpdef])
m4_rename_m4([errprint])
m4_rename_m4([esyscmd])
m4_rename_m4([eval])
m4_rename_m4([format])
m4_rename_m4([ifdef])
m4_rename([ifelse], [m4_if])
m4_undefine([include])
m4_rename_m4([incr])
m4_rename_m4([index])
m4_rename_m4([indir])
m4_rename_m4([len])
m4_rename([m4exit], [m4_exit])
m4_rename([m4wrap], [m4_wrap])
m4_rename_m4([maketemp])
m4_rename([patsubst], [m4_bpatsubst])
m4_undefine([popdef])
m4_rename_m4([pushdef])
m4_rename([regexp], [m4_bregexp])
m4_rename_m4([shift])
m4_undefine([sinclude])
m4_rename_m4([substr])
m4_rename_m4([symbols])
m4_rename_m4([syscmd])
m4_rename_m4([sysval])
m4_rename_m4([traceoff])
m4_rename_m4([traceon])
m4_rename_m4([translit])
m4_undefine([undivert])


## ------------------- ##
## 2. Error messages.  ##
## ------------------- ##


# m4_location
# -----------
m4_define([m4_location],
[__file__:__line__])


# m4_errprintn(MSG)
# -----------------
# Same as `errprint', but with the missing end of line.
m4_define([m4_errprintn],
[m4_errprint([$1
])])


# m4_warning(MSG)
# ---------------
# Warn the user.
m4_define([m4_warning],
[m4_errprintn(m4_location[: warning: $1])])


# m4_fatal(MSG, [EXIT-STATUS])
# ----------------------------
# Fatal the user.                                                      :)
m4_define([m4_fatal],
[m4_errprintn(m4_location[: error: $1])dnl
m4_expansion_stack_dump()dnl
m4_exit(m4_if([$2],, 1, [$2]))])


# m4_assert(EXPRESSION, [EXIT-STATUS = 1])
# ----------------------------------------
# This macro ensures that EXPRESSION evaluates to true, and exits if
# EXPRESSION evaluates to false.
m4_define([m4_assert],
[m4_if(m4_eval([$1]), 0,
       [m4_fatal([assert failed: $1], [$2])])])



## ------------- ##
## 3. Warnings.  ##
## ------------- ##


# _m4_warn(CATEGORY, MESSAGE, STACK-TRACE)
# ----------------------------------------
# Report a MESSAGE to the user if the CATEGORY of warnings is enabled.
# This is for traces only.
# The STACK-TRACE is a \n-separated list of "LOCATION: MESSAGE".
m4_define([_m4_warn], [])


# m4_warn(CATEGORY, MESSAGE)
# --------------------------
# Report a MESSAGE to the user if the CATEGORY of warnings is enabled.
m4_define([m4_warn],
[_m4_warn([$1], [$2],
m4_ifdef([m4_expansion_stack],
         [m4_defn([m4_expansion_stack])
m4_location[: the top level]]))dnl
])



## ------------------- ##
## 4. File inclusion.  ##
## ------------------- ##


# We also want to neutralize include (and sinclude for symmetry),
# but we want to extend them slightly: warn when a file is included
# several times.  This is in general a dangerous operation because
# quite nobody quotes the first argument of m4_define.
#
# For instance in the following case:
#   m4_define(foo, [bar])
# then a second reading will turn into
#   m4_define(bar, [bar])
# which is certainly not what was meant.

# m4_include_unique(FILE)
# -----------------------
# Declare that the FILE was loading; and warn if it has already
# been included.
m4_define([m4_include_unique],
[m4_ifdef([m4_include($1)],
	  [m4_warn([syntax], [file `$1' included several times])])dnl
m4_define([m4_include($1)])])


# m4_include(FILE)
# ----------------
# As the builtin include, but warns against multiple inclusions.
m4_define([m4_include],
[m4_include_unique([$1])dnl
m4_builtin([include], [$1])])


# m4_sinclude(FILE)
# -----------------
# As the builtin sinclude, but warns against multiple inclusions.
m4_define([m4_sinclude],
[m4_include_unique([$1])dnl
m4_builtin([sinclude], [$1])])



## ------------------------------------ ##
## 5. Additional branching constructs.  ##
## ------------------------------------ ##

# Both `m4_ifval' and `m4_ifset' tests against the empty string.  The
# difference is that `m4_ifset' is specialized on macros.
#
# In case of arguments of macros, eg $[1], it makes little difference.
# In the case of a macro `FOO', you don't want to check `m4_ifval(FOO,
# TRUE)', because if `FOO' expands with commas, there is a shifting of
# the arguments.  So you want to run `m4_ifval([FOO])', but then you just
# compare the *string* `FOO' against `', which, of course fails.
#
# So you want a variation of `m4_ifset' that expects a macro name as $[1].
# If this macro is both defined and defined to a non empty value, then
# it runs TRUE etc.


# m4_ifval(COND, [IF-TRUE], [IF-FALSE])
# -------------------------------------
# If COND is not the empty string, expand IF-TRUE, otherwise IF-FALSE.
# Comparable to m4_ifdef.
m4_define([m4_ifval],
[m4_if([$1], [], [$3], [$2])])


# m4_n(TEXT)
# ----------
# If TEXT is not empty, return TEXT and a new line, otherwise nothing.
m4_define([m4_n],
[m4_if([$1],
       [], [],
	   [$1
])])


# m4_ifvaln(COND, [IF-TRUE], [IF-FALSE])
# --------------------------------------
# Same as `m4_ifval', but add an extra newline to IF-TRUE or IF-FALSE
# unless that argument is empty.
m4_define([m4_ifvaln],
[m4_if([$1],
       [],   [m4_n([$3])],
	     [m4_n([$2])])])


# m4_ifset(MACRO, [IF-TRUE], [IF-FALSE])
# --------------------------------------
# If MACRO has no definition, or of its definition is the empty string,
# expand IF-FALSE, otherwise IF-TRUE.
m4_define([m4_ifset],
[m4_ifdef([$1],
	  [m4_ifval(m4_defn([$1]), [$2], [$3])],
	  [$3])])


# m4_ifndef(NAME, [IF-NOT-DEFINED], [IF-DEFINED])
# -----------------------------------------------
m4_define([m4_ifndef],
[m4_ifdef([$1], [$3], [$2])])


# m4_case(SWITCH, VAL1, IF-VAL1, VAL2, IF-VAL2, ..., DEFAULT)
# -----------------------------------------------------------
# m4 equivalent of
# switch (SWITCH)
# {
#   case VAL1:
#     IF-VAL1;
#     break;
#   case VAL2:
#     IF-VAL2;
#     break;
#   ...
#   default:
#     DEFAULT;
#     break;
# }.
# All the values are optional, and the macro is robust to active
# symbols properly quoted.
m4_define([m4_case],
[m4_if([$#], 0, [],
       [$#], 1, [],
       [$#], 2, [$2],
       [$1], [$2], [$3],
       [$0([$1], m4_shiftn(3, $@))])])


# m4_bmatch(SWITCH, RE1, VAL1, RE2, VAL2, ..., DEFAULT)
# -----------------------------------------------------
# m4 equivalent of
#
# if (SWITCH =~ RE1)
#   VAL1;
# elif (SWITCH =~ RE2)
#   VAL2;
# elif ...
#   ...
# else
#   DEFAULT
#
# All the values are optional, and the macro is robust to active symbols
# properly quoted.
m4_define([m4_bmatch],
[m4_if([$#], 0, [m4_fatal([$0: too few arguments: $#])],
       [$#], 1, [m4_fatal([$0: too few arguments: $#: $1])],
       [$#], 2, [$2],
       [m4_if(m4_bregexp([$1], [$2]), -1, [$0([$1], m4_shiftn(3, $@))],
	      [$3])])])


# m4_car(LIST)
# m4_cdr(LIST)
# ------------
# Manipulate m4 lists.
m4_define([m4_car], [[$1]])
m4_define([m4_cdr],
[m4_if([$#], 0, [m4_fatal([$0: cannot be called without arguments])],
       [$#], 1, [],
       [m4_dquote(m4_shift($@))])])


# m4_map(MACRO, LIST)
# -------------------
# Invoke MACRO($1), MACRO($2) etc. where $1, $2... are the elements
# of LIST (which can be lists themselves, for multiple arguments MACROs).
m4_define([m4_fst], [$1])
m4_define([m4_map],
[m4_if([$2], [[]], [],
       [_m4_map([$1], [$2])])])
m4_define([_m4_map],
[m4_ifval([$2],
	  [$1(m4_fst($2))[]_m4_map([$1], m4_cdr($2))])])


# m4_map_sep(MACRO, SEPARATOR, LIST)
# ----------------------------------
# Invoke MACRO($1), SEPARATOR, MACRO($2), ..., MACRO($N) where $1, $2... $N
# are the elements of LIST (which can be lists themselves, for multiple
# arguments MACROs).
m4_define([m4_map_sep],
[m4_if([$3], [[]], [],
       [$1(m4_fst($3))[]_m4_map([$2[]$1], m4_cdr($3))])])


## ---------------------------------------- ##
## 6. Enhanced version of some primitives.  ##
## ---------------------------------------- ##

# m4_bpatsubsts(STRING, RE1, SUBST1, RE2, SUBST2, ...)
# ----------------------------------------------------
# m4 equivalent of
#
#   $_ = STRING;
#   s/RE1/SUBST1/g;
#   s/RE2/SUBST2/g;
#   ...
#
# All the values are optional, and the macro is robust to active symbols
# properly quoted.
#
# I would have liked to name this macro `m4_bpatsubst', unfortunately,
# due to quotation problems, I need to double quote $1 below, therefore
# the anchors are broken :(  I can't let users be trapped by that.
m4_define([m4_bpatsubsts],
[m4_if([$#], 0, [m4_fatal([$0: too few arguments: $#])],
       [$#], 1, [m4_fatal([$0: too few arguments: $#: $1])],
       [$#], 2, [m4_builtin([patsubst], $@)],
       [$0(m4_builtin([patsubst], [[$1]], [$2], [$3]),
	   m4_shiftn(3, $@))])])



# m4_do(STRING, ...)
# ------------------
# This macro invokes all its arguments (in sequence, of course).  It is
# useful for making your macros more structured and readable by dropping
# unnecessary dnl's and have the macros indented properly.
m4_define([m4_do],
[m4_if($#, 0, [],
       $#, 1, [$1],
       [$1[]m4_do(m4_shift($@))])])


# m4_define_default(MACRO, VALUE)
# -------------------------------
# If MACRO is undefined, set it to VALUE.
m4_define([m4_define_default],
[m4_ifndef([$1], [m4_define($@)])])


# m4_default(EXP1, EXP2)
# ----------------------
# Returns EXP1 if non empty, otherwise EXP2.
m4_define([m4_default],
[m4_ifval([$1], [$1], [$2])])


# m4_defn(NAME)
# -------------
# Unlike to the original, don't tolerate popping something which is
# undefined.
m4_define([m4_defn],
[m4_ifndef([$1],
	   [m4_fatal([$0: undefined macro: $1])])dnl
m4_builtin([defn], $@)])


# _m4_dumpdefs_up(NAME)
# ---------------------
m4_define([_m4_dumpdefs_up],
[m4_ifdef([$1],
	  [m4_pushdef([_m4_dumpdefs], m4_defn([$1]))dnl
m4_dumpdef([$1])dnl
m4_popdef([$1])dnl
_m4_dumpdefs_up([$1])])])


# _m4_dumpdefs_down(NAME)
# -----------------------
m4_define([_m4_dumpdefs_down],
[m4_ifdef([_m4_dumpdefs],
	  [m4_pushdef([$1], m4_defn([_m4_dumpdefs]))dnl
m4_popdef([_m4_dumpdefs])dnl
_m4_dumpdefs_down([$1])])])


# m4_dumpdefs(NAME)
# -----------------
# Similar to `m4_dumpdef(NAME)', but if NAME was m4_pushdef'ed, display its
# value stack (most recent displayed first).
m4_define([m4_dumpdefs],
[_m4_dumpdefs_up([$1])dnl
_m4_dumpdefs_down([$1])])


# m4_popdef(NAME)
# ---------------
# Unlike to the original, don't tolerate popping something which is
# undefined.
m4_define([m4_popdef],
[m4_ifndef([$1],
	   [m4_fatal([$0: undefined macro: $1])])dnl
m4_builtin([popdef], $@)])


# m4_quote(ARGS)
# --------------
# Return ARGS as a single arguments.
#
# It is important to realize the difference between `m4_quote(exp)' and
# `[exp]': in the first case you obtain the quoted *result* of the
# expansion of EXP, while in the latter you just obtain the string
# `exp'.
m4_define([m4_quote],  [[$*]])
m4_define([m4_dquote],  [[$@]])


# m4_noquote(STRING)
# ------------------
# Return the result of ignoring all quotes in STRING and invoking the
# macros it contains.  Amongst other things useful for enabling macro
# invocations inside strings with [] blocks (for instance regexps and
# help-strings).
m4_define([m4_noquote],
[m4_changequote(-=<{,}>=-)$1-=<{}>=-m4_changequote([,])])


# m4_shiftn(N, ...)
# -----------------
# Returns ... shifted N times.  Useful for recursive "varargs" constructs.
m4_define([m4_shiftn],
[m4_assert(($1 >= 0) && ($# > $1))dnl
_m4_shiftn($@)])

m4_define([_m4_shiftn],
[m4_if([$1], 0,
       [m4_shift($@)],
       [_m4_shiftn(m4_eval([$1]-1), m4_shift(m4_shift($@)))])])


# m4_undefine(NAME)
# -----------------
# Unlike to the original, don't tolerate undefining something which is
# undefined.
m4_define([m4_undefine],
[m4_ifndef([$1],
	   [m4_fatal([$0: undefined macro: $1])])dnl
m4_builtin([undefine], $@)])


## -------------------------- ##
## 7. Implementing m4 loops.  ##
## -------------------------- ##


# m4_for(VARIABLE, FIRST, LAST, [STEP = +/-1], EXPRESSION)
# --------------------------------------------------------
# Expand EXPRESSION defining VARIABLE to FROM, FROM + 1, ..., TO.
# Both limits are included, and bounds are checked for consistency.
m4_define([m4_for],
[m4_case(m4_sign(m4_eval($3 - $2)),
	 1, [m4_assert(m4_sign(m4_default($4, 1)) == 1)],
	-1, [m4_assert(m4_sign(m4_default($4, -1)) == -1)])dnl
m4_pushdef([$1], [$2])dnl
m4_if(m4_eval([$3 > $2]), 1,
      [_m4_for([$1], [$3], m4_default([$4], 1), [$5])],
      [_m4_for([$1], [$3], m4_default([$4], -1), [$5])])dnl
m4_popdef([$1])])


# _m4_for(VARIABLE, FIRST, LAST, STEP, EXPRESSION)
# ------------------------------------------------
# Core of the loop, no consistency checks.
m4_define([_m4_for],
[$4[]dnl
m4_if($1, [$2], [],
      [m4_define([$1], m4_eval($1+[$3]))_m4_for([$1], [$2], [$3], [$4])])])


# Implementing `foreach' loops in m4 is much more tricky than it may
# seem.  Actually, the example of a `foreach' loop in the m4
# documentation is wrong: it does not quote the arguments properly,
# which leads to undesirable expansions.
#
# The example in the documentation is:
#
# | # foreach(VAR, (LIST), STMT)
# | m4_define([foreach],
# |        [m4_pushdef([$1])_foreach([$1], [$2], [$3])m4_popdef([$1])])
# | m4_define([_arg1], [$1])
# | m4_define([_foreach],
# |	       [m4_if([$2], [()], ,
# |		     [m4_define([$1], _arg1$2)$3[]_foreach([$1],
# |                                                        (shift$2),
# |                                                        [$3])])])
#
# But then if you run
#
# | m4_define(a, 1)
# | m4_define(b, 2)
# | m4_define(c, 3)
# | foreach([f], [([a], [(b], [c)])], [echo f
# | ])
#
# it gives
#
#  => echo 1
#  => echo (2,3)
#
# which is not what is expected.
#
# Of course the problem is that many quotes are missing.  So you add
# plenty of quotes at random places, until you reach the expected
# result.  Alternatively, if you are a quoting wizard, you directly
# reach the following implementation (but if you really did, then
# apply to the maintenance of m4sugar!).
#
# | # foreach(VAR, (LIST), STMT)
# | m4_define([foreach], [m4_pushdef([$1])_foreach($@)m4_popdef([$1])])
# | m4_define([_arg1], [[$1]])
# | m4_define([_foreach],
# |  [m4_if($2, [()], ,
# |	     [m4_define([$1], [_arg1$2])$3[]_foreach([$1],
# |                                                 [(shift$2)],
# |                                                 [$3])])])
#
# which this time answers
#
#  => echo a
#  => echo (b
#  => echo c)
#
# Bingo!
#
# Well, not quite.
#
# With a better look, you realize that the parens are more a pain than
# a help: since anyway you need to quote properly the list, you end up
# with always using an outermost pair of parens and an outermost pair
# of quotes.  Rejecting the parens both eases the implementation, and
# simplifies the use:
#
# | # foreach(VAR, (LIST), STMT)
# | m4_define([foreach], [m4_pushdef([$1])_foreach($@)m4_popdef([$1])])
# | m4_define([_arg1], [$1])
# | m4_define([_foreach],
# |  [m4_if($2, [], ,
# |	     [m4_define([$1], [_arg1($2)])$3[]_foreach([$1],
# |                                                   [shift($2)],
# |                                                   [$3])])])
#
#
# Now, just replace the `$2' with `m4_quote($2)' in the outer `m4_if'
# to improve robustness, and you come up with a quite satisfactory
# implementation.


# m4_foreach(VARIABLE, LIST, EXPRESSION)
# --------------------------------------
#
# Expand EXPRESSION assigning each value of the LIST to VARIABLE.
# LIST should have the form `item_1, item_2, ..., item_n', i.e. the
# whole list must *quoted*.  Quote members too if you don't want them
# to be expanded.
#
# This macro is robust to active symbols:
#      | m4_define(active, [ACT, IVE])
#      | m4_foreach(Var, [active, active], [-Var-])
#     => -ACT--IVE--ACT--IVE-
#
#      | m4_foreach(Var, [[active], [active]], [-Var-])
#     => -ACT, IVE--ACT, IVE-
#
#      | m4_foreach(Var, [[[active]], [[active]]], [-Var-])
#     => -active--active-
m4_define([m4_foreach],
[m4_pushdef([$1])_m4_foreach($@)m4_popdef([$1])])

m4_define([_m4_foreach],
[m4_ifval([$2],
	  [m4_define([$1], m4_car($2))$3[]dnl
_m4_foreach([$1], m4_cdr($2), [$3])])])


# m4_foreach_w(VARIABLE, LIST, EXPRESSION)
# ----------------------------------------
#
# Like m4_foreach, but the list is whitespace separated.
#
# This macro is robust to active symbols:
#    m4_foreach_w([Var], [ active
#    b	act\
#    ive  ], [-Var-])end
#    => -active--b--active-end
#
m4_define([m4_foreach_w],
[m4_foreach([$1], m4_split(m4_normalize([$2])), [$3])])



## --------------------------- ##
## 8. More diversion support.  ##
## --------------------------- ##


# _m4_divert(DIVERSION-NAME or NUMBER)
# ------------------------------------
# If DIVERSION-NAME is the name of a diversion, return its number,
# otherwise if it is a NUMBER return it.
m4_define([_m4_divert],
[m4_ifdef([_m4_divert($1)],
	  [m4_indir([_m4_divert($1)])],
	  [$1])])

# KILL is only used to suppress output.
m4_define([_m4_divert(KILL)],           -1)


# _m4_divert_n_stack
# ------------------
# Print m4_divert_stack with newline prepended, if it's nonempty.
m4_define([_m4_divert_n_stack],
[m4_ifdef([m4_divert_stack], [
m4_defn([m4_divert_stack])])])


# m4_divert(DIVERSION-NAME)
# -------------------------
# Change the diversion stream to DIVERSION-NAME.
m4_define([m4_divert],
[m4_define([m4_divert_stack], m4_location[: $0: $1]_m4_divert_n_stack)dnl
m4_builtin([divert], _m4_divert([$1]))dnl
])


# m4_divert_push(DIVERSION-NAME)
# ------------------------------
# Change the diversion stream to DIVERSION-NAME, while stacking old values.
m4_define([m4_divert_push],
[m4_pushdef([m4_divert_stack], m4_location[: $0: $1]_m4_divert_n_stack)dnl
m4_pushdef([_m4_divert_diversion], [$1])dnl
m4_builtin([divert], _m4_divert([$1]))dnl
])


# m4_divert_pop([DIVERSION-NAME])
# -------------------------------
# Change the diversion stream to its previous value, unstacking it.
# If specified, verify we left DIVERSION-NAME.
# When we pop the last value from the stack, we divert to -1.
m4_define([m4_divert_pop],
[m4_ifndef([_m4_divert_diversion],
           [m4_fatal([too many m4_divert_pop])])dnl
m4_if([$1], [], [],
      [$1], m4_defn([_m4_divert_diversion]), [],
      [m4_fatal([$0($1): diversion mismatch: ]_m4_divert_n_stack)])dnl
m4_popdef([m4_divert_stack])dnl
m4_popdef([_m4_divert_diversion])dnl
m4_builtin([divert],
	   m4_ifdef([_m4_divert_diversion],
		    [_m4_divert(m4_defn([_m4_divert_diversion]))],
		    -1))dnl
])


# m4_divert_text(DIVERSION-NAME, CONTENT)
# ---------------------------------------
# Output CONTENT into DIVERSION-NAME (which may be a number actually).
# An end of line is appended for free to CONTENT.
m4_define([m4_divert_text],
[m4_divert_push([$1])dnl
$2
m4_divert_pop([$1])dnl
])


# m4_divert_once(DIVERSION-NAME, CONTENT)
# ---------------------------------------
# Output once CONTENT into DIVERSION-NAME (which may be a number
# actually).  An end of line is appended for free to CONTENT.
m4_define([m4_divert_once],
[m4_expand_once([m4_divert_text([$1], [$2])])])


# m4_undivert(DIVERSION-NAME)
# ---------------------------
# Undivert DIVERSION-NAME.
m4_define([m4_undivert],
[m4_builtin([undivert], _m4_divert([$1]))])


## -------------------------------------------- ##
## 8. Defining macros with bells and whistles.  ##
## -------------------------------------------- ##

# `m4_defun' is basically `m4_define' but it equips the macro with the
# needed machinery for `m4_require'.  A macro must be m4_defun'd if
# either it is m4_require'd, or it m4_require's.
#
# Two things deserve attention and are detailed below:
#  1. Implementation of m4_require
#  2. Keeping track of the expansion stack
#
# 1. Implementation of m4_require
# ===============================
#
# Of course m4_defun AC_PROVIDE's the macro, so that a macro which has
# been expanded is not expanded again when m4_require'd, but the
# difficult part is the proper expansion of macros when they are
# m4_require'd.
#
# The implementation is based on two ideas, (i) using diversions to
# prepare the expansion of the macro and its dependencies (by Franc,ois
# Pinard), and (ii) expand the most recently m4_require'd macros _after_
# the previous macros (by Axel Thimm).
#
#
# The first idea: why using diversions?
# -------------------------------------
#
# When a macro requires another, the other macro is expanded in new
# diversion, GROW.  When the outer macro is fully expanded, we first
# undivert the most nested diversions (GROW - 1...), and finally
# undivert GROW.  To understand why we need several diversions,
# consider the following example:
#
# | m4_defun([TEST1], [Test...REQUIRE([TEST2])1])
# | m4_defun([TEST2], [Test...REQUIRE([TEST3])2])
# | m4_defun([TEST3], [Test...3])
#
# Because m4_require is not required to be first in the outer macros, we
# must keep the expansions of the various level of m4_require separated.
# Right before executing the epilogue of TEST1, we have:
#
#	   GROW - 2: Test...3
#	   GROW - 1: Test...2
#	   GROW:     Test...1
#	   BODY:
#
# Finally the epilogue of TEST1 undiverts GROW - 2, GROW - 1, and
# GROW into the regular flow, BODY.
#
#	   GROW - 2:
#	   GROW - 1:
#	   GROW:
#	   BODY:        Test...3; Test...2; Test...1
#
# (The semicolons are here for clarification, but of course are not
# emitted.)  This is what Autoconf 2.0 (I think) to 2.13 (I'm sure)
# implement.
#
#
# The second idea: first required first out
# -----------------------------------------
#
# The natural implementation of the idea above is buggy and produces
# very surprising results in some situations.  Let's consider the
# following example to explain the bug:
#
# | m4_defun([TEST1],  [REQUIRE([TEST2a])REQUIRE([TEST2b])])
# | m4_defun([TEST2a], [])
# | m4_defun([TEST2b], [REQUIRE([TEST3])])
# | m4_defun([TEST3],  [REQUIRE([TEST2a])])
# |
# | AC_INIT
# | TEST1
#
# The dependencies between the macros are:
#
#		 3 --- 2b
#		/        \              is m4_require'd by
#	       /          \       left -------------------- right
#	    2a ------------ 1
#
# If you strictly apply the rules given in the previous section you get:
#
#	   GROW - 2: TEST3
#	   GROW - 1: TEST2a; TEST2b
#	   GROW:     TEST1
#	   BODY:
#
# (TEST2a, although required by TEST3 is not expanded in GROW - 3
# because is has already been expanded before in GROW - 1, so it has
# been AC_PROVIDE'd, so it is not expanded again) so when you undivert
# the stack of diversions, you get:
#
#	   GROW - 2:
#	   GROW - 1:
#	   GROW:
#	   BODY:        TEST3; TEST2a; TEST2b; TEST1
#
# i.e., TEST2a is expanded after TEST3 although the latter required the
# former.
#
# Starting from 2.50, uses an implementation provided by Axel Thimm.
# The idea is simple: the order in which macros are emitted must be the
# same as the one in which macro are expanded.  (The bug above can
# indeed be described as: a macro has been AC_PROVIDE'd, but it is
# emitted after: the lack of correlation between emission and expansion
# order is guilty).
#
# How to do that?  You keeping the stack of diversions to elaborate the
# macros, but each time a macro is fully expanded, emit it immediately.
#
# In the example above, when TEST2a is expanded, but it's epilogue is
# not run yet, you have:
#
#	   GROW - 2:
#	   GROW - 1: TEST2a
#	   GROW:     Elaboration of TEST1
#	   BODY:
#
# The epilogue of TEST2a emits it immediately:
#
#	   GROW - 2:
#	   GROW - 1:
#	   GROW:     Elaboration of TEST1
#	   BODY:     TEST2a
#
# TEST2b then requires TEST3, so right before the epilogue of TEST3, you
# have:
#
#	   GROW - 2: TEST3
#	   GROW - 1: Elaboration of TEST2b
#	   GROW:     Elaboration of TEST1
#	   BODY:      TEST2a
#
# The epilogue of TEST3 emits it:
#
#	   GROW - 2:
#	   GROW - 1: Elaboration of TEST2b
#	   GROW:     Elaboration of TEST1
#	   BODY:     TEST2a; TEST3
#
# TEST2b is now completely expanded, and emitted:
#
#	   GROW - 2:
#	   GROW - 1:
#	   GROW:     Elaboration of TEST1
#	   BODY:     TEST2a; TEST3; TEST2b
#
# and finally, TEST1 is finished and emitted:
#
#	   GROW - 2:
#	   GROW - 1:
#	   GROW:
#	   BODY:     TEST2a; TEST3; TEST2b: TEST1
#
# The idea is simple, but the implementation is a bit evolved.  If you
# are like me, you will want to see the actual functioning of this
# implementation to be convinced.  The next section gives the full
# details.
#
#
# The Axel Thimm implementation at work
# -------------------------------------
#
# We consider the macros above, and this configure.ac:
#
#	    AC_INIT
#	    TEST1
#
# You should keep the definitions of _m4_defun_pro, _m4_defun_epi, and
# m4_require at hand to follow the steps.
#
# This implements tries not to assume that the current diversion is
# BODY, so as soon as a macro (m4_defun'd) is expanded, we first
# record the current diversion under the name _m4_divert_dump (denoted
# DUMP below for short).  This introduces an important difference with
# the previous versions of Autoconf: you cannot use m4_require if you
# are not inside an m4_defun'd macro, and especially, you cannot
# m4_require directly from the top level.
#
# We have not tried to simulate the old behavior (better yet, we
# diagnose it), because it is too dangerous: a macro m4_require'd from
# the top level is expanded before the body of `configure', i.e., before
# any other test was run.  I let you imagine the result of requiring
# AC_STDC_HEADERS for instance, before AC_PROG_CC was actually run....
#
# After AC_INIT was run, the current diversion is BODY.
# * AC_INIT was run
#   DUMP:                undefined
#   diversion stack:     BODY |-
#
# * TEST1 is expanded
# The prologue of TEST1 sets _m4_divert_dump, which is the diversion
# where the current elaboration will be dumped, to the current
# diversion.  It also m4_divert_push to GROW, where the full
# expansion of TEST1 and its dependencies will be elaborated.
#   DUMP:        BODY
#   BODY:        empty
#   diversions:  GROW, BODY |-
#
# * TEST1 requires TEST2a
# _m4_require_call m4_divert_pushes another temporary diversion,
# GROW - 1, and expands TEST2a in there.
#   DUMP:        BODY
#   BODY:        empty
#   GROW - 1:    TEST2a
#   diversions:  GROW - 1, GROW, BODY |-
# Than the content of the temporary diversion is moved to DUMP and the
# temporary diversion is popped.
#   DUMP:        BODY
#   BODY:        TEST2a
#   diversions:  GROW, BODY |-
#
# * TEST1 requires TEST2b
# Again, _m4_require_call pushes GROW - 1 and heads to expand TEST2b.
#   DUMP:        BODY
#   BODY:        TEST2a
#   diversions:  GROW - 1, GROW, BODY |-
#
# * TEST2b requires TEST3
# _m4_require_call pushes GROW - 2 and expands TEST3 here.
# (TEST3 requires TEST2a, but TEST2a has already been m4_provide'd, so
# nothing happens.)
#   DUMP:        BODY
#   BODY:        TEST2a
#   GROW - 2:    TEST3
#   diversions:  GROW - 2, GROW - 1, GROW, BODY |-
# Than the diversion is appended to DUMP, and popped.
#   DUMP:        BODY
#   BODY:        TEST2a; TEST3
#   diversions:  GROW - 1, GROW, BODY |-
#
# * TEST1 requires TEST2b (contd.)
# The content of TEST2b is expanded...
#   DUMP:        BODY
#   BODY:        TEST2a; TEST3
#   GROW - 1:    TEST2b,
#   diversions:  GROW - 1, GROW, BODY |-
# ... and moved to DUMP.
#   DUMP:        BODY
#   BODY:        TEST2a; TEST3; TEST2b
#   diversions:  GROW, BODY |-
#
# * TEST1 is expanded: epilogue
# TEST1's own content is in GROW...
#   DUMP:        BODY
#   BODY:        TEST2a; TEST3; TEST2b
#   GROW:        TEST1
#   diversions:  BODY |-
# ... and it's epilogue moves it to DUMP and then undefines DUMP.
#   DUMP:       undefined
#   BODY:       TEST2a; TEST3; TEST2b; TEST1
#   diversions: BODY |-
#
#
# 2. Keeping track of the expansion stack
# =======================================
#
# When M4 expansion goes wrong it is often extremely hard to find the
# path amongst macros that drove to the failure.  What is needed is
# the stack of macro `calls'. One could imagine that GNU M4 would
# maintain a stack of macro expansions, unfortunately it doesn't, so
# we do it by hand.  This is of course extremely costly, but the help
# this stack provides is worth it.  Nevertheless to limit the
# performance penalty this is implemented only for m4_defun'd macros,
# not for define'd macros.
#
# The scheme is simplistic: each time we enter an m4_defun'd macros,
# we prepend its name in m4_expansion_stack, and when we exit the
# macro, we remove it (thanks to pushdef/popdef).
#
# In addition, we want to detect circular m4_require dependencies.
# Each time we expand a macro FOO we define _m4_expanding(FOO); and
# m4_require(BAR) simply checks whether _m4_expanding(BAR) is defined.


# m4_expansion_stack_push(TEXT)
# -----------------------------
m4_define([m4_expansion_stack_push],
[m4_pushdef([m4_expansion_stack],
	    [$1]m4_ifdef([m4_expansion_stack], [
m4_defn([m4_expansion_stack])]))])


# m4_expansion_stack_pop
# ----------------------
m4_define([m4_expansion_stack_pop],
[m4_popdef([m4_expansion_stack])])


# m4_expansion_stack_dump
# -----------------------
# Dump the expansion stack.
m4_define([m4_expansion_stack_dump],
[m4_ifdef([m4_expansion_stack],
	  [m4_errprintn(m4_defn([m4_expansion_stack]))])dnl
m4_errprintn(m4_location[: the top level])])


# _m4_divert(GROW)
# ----------------
# This diversion is used by the m4_defun/m4_require machinery.  It is
# important to keep room before GROW because for each nested
# AC_REQUIRE we use an additional diversion (i.e., two m4_require's
# will use GROW - 2.  More than 3 levels has never seemed to be
# needed.)
#
# ...
# - GROW - 2
#   m4_require'd code, 2 level deep
# - GROW - 1
#   m4_require'd code, 1 level deep
# - GROW
#   m4_defun'd macros are elaborated here.

m4_define([_m4_divert(GROW)],       10000)


# _m4_defun_pro(MACRO-NAME)
# -------------------------
# The prologue for Autoconf macros.
m4_define([_m4_defun_pro],
[m4_ifndef([m4_expansion_stack], [_m4_defun_pro_outer[]])dnl
m4_expansion_stack_push(m4_defn([m4_location($1)])[: $1 is expanded from...])dnl
m4_pushdef([_m4_expanding($1)])dnl
])

m4_define([_m4_defun_pro_outer],
[m4_copy([_m4_divert_diversion], [_m4_divert_dump])dnl
m4_divert_push([GROW])dnl
])

# _m4_defun_epi(MACRO-NAME)
# -------------------------
# The Epilogue for Autoconf macros.  MACRO-NAME only helps tracing
# the PRO/EPI pairs.
m4_define([_m4_defun_epi],
[m4_popdef([_m4_expanding($1)])dnl
m4_expansion_stack_pop()dnl
m4_ifndef([m4_expansion_stack], [_m4_defun_epi_outer[]])dnl
m4_provide([$1])dnl
])

m4_define([_m4_defun_epi_outer],
[m4_undefine([_m4_divert_dump])dnl
m4_divert_pop([GROW])dnl
m4_undivert([GROW])dnl
])


# m4_defun(NAME, EXPANSION)
# -------------------------
# Define a macro which automatically provides itself.  Add machinery
# so the macro automatically switches expansion to the diversion
# stack if it is not already using it.  In this case, once finished,
# it will bring back all the code accumulated in the diversion stack.
# This, combined with m4_require, achieves the topological ordering of
# macros.  We don't use this macro to define some frequently called
# macros that are not involved in ordering constraints, to save m4
# processing.
m4_define([m4_defun],
[m4_define([m4_location($1)], m4_location)dnl
m4_define([$1],
	  [_m4_defun_pro([$1])$2[]_m4_defun_epi([$1])])])


# m4_defun_once(NAME, EXPANSION)
# ------------------------------
# As m4_defun, but issues the EXPANSION only once, and warns if used
# several times.
m4_define([m4_defun_once],
[m4_define([m4_location($1)], m4_location)dnl
m4_define([$1],
	  [m4_provide_if([$1],
			 [m4_warn([syntax], [$1 invoked multiple times])],
			 [_m4_defun_pro([$1])$2[]_m4_defun_epi([$1])])])])


# m4_pattern_forbid(ERE, [WHY])
# -----------------------------
# Declare that no token matching the extended regular expression ERE
# should be seen in the output but if...
m4_define([m4_pattern_forbid], [])


# m4_pattern_allow(ERE)
# ---------------------
# ... but if that token matches the extended regular expression ERE.
# Both used via traces.
m4_define([m4_pattern_allow], [])


## ----------------------------- ##
## Dependencies between macros.  ##
## ----------------------------- ##


# m4_before(THIS-MACRO-NAME, CALLED-MACRO-NAME)
# ---------------------------------------------
m4_define([m4_before],
[m4_provide_if([$2],
	       [m4_warn([syntax], [$2 was called before $1])])])


# m4_require(NAME-TO-CHECK, [BODY-TO-EXPAND = NAME-TO-CHECK])
# -----------------------------------------------------------
# If NAME-TO-CHECK has never been expanded (actually, if it is not
# m4_provide'd), expand BODY-TO-EXPAND *before* the current macro
# expansion.  Once expanded, emit it in _m4_divert_dump.  Keep track
# of the m4_require chain in m4_expansion_stack.
#
# The normal cases are:
#
# - NAME-TO-CHECK == BODY-TO-EXPAND
#   Which you can use for regular macros with or without arguments, e.g.,
#     m4_require([AC_PROG_CC], [AC_PROG_CC])
#     m4_require([AC_CHECK_HEADERS(limits.h)], [AC_CHECK_HEADERS(limits.h)])
#   which is just the same as
#     m4_require([AC_PROG_CC])
#     m4_require([AC_CHECK_HEADERS(limits.h)])
#
# - BODY-TO-EXPAND == m4_indir([NAME-TO-CHECK])
#   In the case of macros with irregular names.  For instance:
#     m4_require([AC_LANG_COMPILER(C)], [indir([AC_LANG_COMPILER(C)])])
#   which means `if the macro named `AC_LANG_COMPILER(C)' (the parens are
#   part of the name, it is not an argument) has not been run, then
#   call it.'
#   Had you used
#     m4_require([AC_LANG_COMPILER(C)], [AC_LANG_COMPILER(C)])
#   then m4_require would have tried to expand `AC_LANG_COMPILER(C)', i.e.,
#   call the macro `AC_LANG_COMPILER' with `C' as argument.
#
#   You could argue that `AC_LANG_COMPILER', when it receives an argument
#   such as `C' should dispatch the call to `AC_LANG_COMPILER(C)'.  But this
#   `extension' prevents `AC_LANG_COMPILER' from having actual arguments that
#   it passes to `AC_LANG_COMPILER(C)'.
m4_define([m4_require],
[m4_ifdef([_m4_expanding($1)],
	 [m4_fatal([$0: circular dependency of $1])])dnl
m4_ifndef([_m4_divert_dump],
	  [m4_fatal([$0($1): cannot be used outside of an m4_defun'd macro])])dnl
m4_provide_if([$1],
	      [],
	      [_m4_require_call([$1], [$2])])dnl
])


# _m4_require_call(BODY-TO-EXPAND)
# --------------------------------
# If m4_require decides to expand the body, it calls this macro.
m4_define([_m4_require_call],
[m4_define([_m4_divert_grow], m4_decr(_m4_divert_grow))dnl
m4_divert_push(_m4_divert_grow)dnl
m4_default([$2], [$1])
m4_provide_if([$1],
	      [],
	      [m4_warn([syntax],
		       [$1 is m4_require'd but not m4_defun'd])])dnl
m4_divert(m4_defn([_m4_divert_dump]))dnl
m4_undivert(_m4_divert_grow)dnl
m4_divert_pop(_m4_divert_grow)dnl
m4_define([_m4_divert_grow], m4_incr(_m4_divert_grow))dnl
])


# _m4_divert_grow
# ---------------
# The counter for _m4_require_call.
m4_define([_m4_divert_grow], _m4_divert([GROW]))


# m4_expand_once(TEXT, [WITNESS = TEXT])
# --------------------------------------
# If TEXT has never been expanded, expand it *here*.  Use WITNESS as
# as a memory that TEXT has already been expanded.
m4_define([m4_expand_once],
[m4_provide_if(m4_ifval([$2], [[$2]], [[$1]]),
	       [],
	       [m4_provide(m4_ifval([$2], [[$2]], [[$1]]))[]$1])])


# m4_provide(MACRO-NAME)
# ----------------------
m4_define([m4_provide],
[m4_define([m4_provide($1)])])


# m4_provide_if(MACRO-NAME, IF-PROVIDED, IF-NOT-PROVIDED)
# -------------------------------------------------------
# If MACRO-NAME is provided do IF-PROVIDED, else IF-NOT-PROVIDED.
# The purpose of this macro is to provide the user with a means to
# check macros which are provided without letting her know how the
# information is coded.
m4_define([m4_provide_if],
[m4_ifdef([m4_provide($1)],
	  [$2], [$3])])


## -------------------- ##
## 9. Text processing.  ##
## -------------------- ##


# m4_cr_letters
# m4_cr_LETTERS
# m4_cr_Letters
# -------------
m4_define([m4_cr_letters], [abcdefghijklmnopqrstuvwxyz])
m4_define([m4_cr_LETTERS], [ABCDEFGHIJKLMNOPQRSTUVWXYZ])
m4_define([m4_cr_Letters],
m4_defn([m4_cr_letters])dnl
m4_defn([m4_cr_LETTERS])dnl
)


# m4_cr_digits
# ------------
m4_define([m4_cr_digits], [0123456789])


# m4_cr_symbols1 & m4_cr_symbols2
# -------------------------------
m4_define([m4_cr_symbols1],
m4_defn([m4_cr_Letters])dnl
_)

m4_define([m4_cr_symbols2],
m4_defn([m4_cr_symbols1])dnl
m4_defn([m4_cr_digits])dnl
)


# m4_re_escape(STRING)
# --------------------
# Escape RE active characters in STRING.
m4_define([m4_re_escape],
[m4_bpatsubst([$1],
	      [[][*+.?\^$]], [\\\&])])


# m4_re_string
# ------------
# Regexp for `[a-zA-Z_0-9]*'
# m4_dquote provides literal [] for the character class.
m4_define([m4_re_string],
m4_dquote(m4_defn([m4_cr_symbols2]))dnl
[*]dnl
)


# m4_re_word
# ----------
# Regexp for `[a-zA-Z_][a-zA-Z_0-9]*'
m4_define([m4_re_word],
m4_dquote(m4_defn([m4_cr_symbols1]))dnl
m4_defn([m4_re_string])dnl
)


# m4_tolower(STRING)
# m4_toupper(STRING)
# ------------------
# These macros lowercase and uppercase strings.
m4_define([m4_tolower],
[m4_translit([$1], m4_defn([m4_cr_LETTERS]), m4_defn([m4_cr_letters]))])
m4_define([m4_toupper],
[m4_translit([$1], m4_defn([m4_cr_letters]), m4_defn([m4_cr_LETTERS]))])


# m4_split(STRING, [REGEXP])
# --------------------------
#
# Split STRING into an m4 list of quoted elements.  The elements are
# quoted with [ and ].  Beginning spaces and end spaces *are kept*.
# Use m4_strip to remove them.
#
# REGEXP specifies where to split.  Default is [\t ]+.
#
# If STRING is empty, the result is an empty list.
#
# Pay attention to the m4_changequotes.  When m4 reads the definition of
# m4_split, it still has quotes set to [ and ].  Luckily, these are matched
# in the macro body, so the definition is stored correctly.
#
# Also, notice that $1 is quoted twice, since we want the result to
# be quoted.  Then you should understand that the argument of
# patsubst is ``STRING'' (i.e., with additional `` and '').
#
# This macro is safe on active symbols, i.e.:
#   m4_define(active, ACTIVE)
#   m4_split([active active ])end
#   => [active], [active], []end

m4_define([m4_split],
[m4_ifval([$1], [_m4_split($@)])])

m4_define([_m4_split],
[m4_changequote(``, '')dnl
[dnl Can't use m4_default here instead of m4_if, because m4_default uses
dnl [ and ] as quotes.
m4_bpatsubst(````$1'''',
	     m4_if(``$2'',, ``[	 ]+'', ``$2''),
	     ``], ['')]dnl
m4_changequote([, ])])



# m4_flatten(STRING)
# ------------------
# If STRING contains end of lines, replace them with spaces.  If there
# are backslashed end of lines, remove them.  This macro is safe with
# active symbols.
#    m4_define(active, ACTIVE)
#    m4_flatten([active
#    act\
#    ive])end
#    => active activeend
m4_define([m4_flatten],
[m4_translit(m4_bpatsubst([[[$1]]], [\\
]), [
], [ ])])


# m4_strip(STRING)
# ----------------
# Expands into STRING with tabs and spaces singled out into a single
# space, and removing leading and trailing spaces.
#
# This macro is robust to active symbols.
#    m4_define(active, ACTIVE)
#    m4_strip([  active <tab> <tab>active ])end
#    => active activeend
#
# Because we want to preserve active symbols, STRING must be double-quoted.
#
# Then notice the 2 last patterns: they are in charge of removing the
# leading/trailing spaces.  Why not just `[^ ]'?  Because they are
# applied to doubly quoted strings, i.e. more or less [[STRING]].  So
# if there is a leading space in STRING, then it is the *third*
# character, since there are two leading `['; equally for the last pattern.
m4_define([m4_strip],
[m4_bpatsubsts([[$1]],
	       [[	 ]+], [ ],
	       [^\(..\) ],    [\1],
	       [ \(..\)$],    [\1])])


# m4_normalize(STRING)
# --------------------
# Apply m4_flatten and m4_strip to STRING.
#
# The argument is quoted, so that the macro is robust to active symbols:
#
#    m4_define(active, ACTIVE)
#    m4_normalize([  act\
#    ive
#    active ])end
#    => active activeend

m4_define([m4_normalize],
[m4_strip(m4_flatten([$1]))])



# m4_join(SEP, ARG1, ARG2...)
# ---------------------------
# Produce ARG1SEPARG2...SEPARGn.
m4_defun([m4_join],
[m4_case([$#],
	 [1], [],
	 [2], [[$2]],
	 [[$2][$1]$0([$1], m4_shiftn(2, $@))])])



# m4_append(MACRO-NAME, STRING, [SEPARATOR])
# ------------------------------------------
# Redefine MACRO-NAME to hold its former content plus `SEPARATOR`'STRING'
# at the end.  It is valid to use this macro with MACRO-NAME undefined,
# in which case no SEPARATOR is added.  Be aware that the criterion is
# `not being defined', and not `not being empty'.
#
# This macro is robust to active symbols.  It can be used to grow
# strings.
#
#    | m4_define(active, ACTIVE)
#    | m4_append([sentence], [This is an])
#    | m4_append([sentence], [ active ])
#    | m4_append([sentence], [symbol.])
#    | sentence
#    | m4_undefine([active])dnl
#    | sentence
#    => This is an ACTIVE symbol.
#    => This is an active symbol.
#
# It can be used to define hooks.
#
#    | m4_define(active, ACTIVE)
#    | m4_append([hooks], [m4_define([act1], [act2])])
#    | m4_append([hooks], [m4_define([act2], [active])])
#    | m4_undefine([active])
#    | act1
#    | hooks
#    | act1
#    => act1
#    =>
#    => active
m4_define([m4_append],
[m4_define([$1],
	   m4_ifdef([$1], [m4_defn([$1])$3])[$2])])

# m4_prepend(MACRO-NAME, STRING, [SEPARATOR])
# -------------------------------------------
# Same, but prepend.
m4_define([m4_prepend],
[m4_define([$1],
	   [$2]m4_ifdef([$1], [$3[]m4_defn([$1])]))])

# m4_append_uniq(MACRO-NAME, STRING, [SEPARATOR])
# -----------------------------------------------
# As `m4_append', but append only if not yet present.
m4_define([m4_append_uniq],
[m4_ifdef([$1],
	  [m4_bmatch([$3]m4_defn([$1])[$3], m4_re_escape([$3$2$3]), [],
		     [m4_append($@)])],
	  [m4_append($@)])])


# m4_text_wrap(STRING, [PREFIX], [FIRST-PREFIX], [WIDTH])
# -------------------------------------------------------
# Expands into STRING wrapped to hold in WIDTH columns (default = 79).
# If PREFIX is given, each line is prefixed with it.  If FIRST-PREFIX is
# specified, then the first line is prefixed with it.  As a special case,
# if the length of FIRST-PREFIX is greater than that of PREFIX, then
# FIRST-PREFIX will be left alone on the first line.
#
# Typical outputs are:
#
# m4_text_wrap([Short string */], [   ], [/* ], 20)
#  => /* Short string */
#
# m4_text_wrap([Much longer string */], [   ], [/* ], 20)
#  => /* Much longer
#  =>    string */
#
# m4_text_wrap([Short doc.], [          ], [  --short ], 30)
#  =>   --short Short doc.
#
# m4_text_wrap([Short doc.], [          ], [  --too-wide ], 30)
#  =>   --too-wide
#  =>           Short doc.
#
# m4_text_wrap([Super long documentation.], [          ], [  --too-wide ], 30)
#  =>   --too-wide
#  =>      Super long
#  =>      documentation.
#
# FIXME: there is no checking of a longer PREFIX than WIDTH, but do
# we really want to bother with people trying each single corner
# of a software?
#
# more important:
# FIXME: handle quadrigraphs correctly, both in TEXT and in FIRST_PREFIX.
#
# This macro does not leave a trailing space behind the last word,
# what complicates it a bit.  The algorithm is stupid simple: all the
# words are preceded by m4_Separator which is defined to empty for the
# first word, and then ` ' (single space) for all the others.
m4_define([m4_text_wrap],
[m4_pushdef([m4_Prefix], [$2])dnl
m4_pushdef([m4_Prefix1], m4_default([$3], [m4_Prefix]))dnl
m4_pushdef([m4_Width], m4_default([$4], 79))dnl
m4_pushdef([m4_Cursor], m4_len(m4_Prefix1))dnl
m4_pushdef([m4_Separator], [])dnl
m4_Prefix1[]dnl
m4_if(m4_eval(m4_Cursor > m4_len(m4_Prefix)),
      1, [m4_define([m4_Cursor], m4_len(m4_Prefix))
m4_Prefix])[]dnl
m4_foreach_w([m4_Word], [$1],
[m4_define([m4_Cursor], m4_eval(m4_Cursor + m4_len(m4_defn([m4_Word])) + 1))dnl
dnl New line if too long, else insert a space unless it is the first
dnl of the words.
m4_if(m4_eval(m4_Cursor > m4_Width),
      1, [m4_define([m4_Cursor],
		    m4_eval(m4_len(m4_Prefix) + m4_len(m4_defn([m4_Word])) + 1))]
m4_Prefix,
       [m4_Separator])[]dnl
m4_defn([m4_Word])[]dnl
m4_define([m4_Separator], [ ])])dnl
m4_popdef([m4_Separator])dnl
m4_popdef([m4_Cursor])dnl
m4_popdef([m4_Width])dnl
m4_popdef([m4_Prefix1])dnl
m4_popdef([m4_Prefix])dnl
])


# m4_text_box(MESSAGE, [FRAME-CHARACTER = `-'])
# ---------------------------------------------
m4_define([m4_text_box],
[@%:@@%:@ m4_bpatsubst([$1], [.], m4_if([$2], [], [[-]], [[$2]])) @%:@@%:@
@%:@@%:@ $1 @%:@@%:@
@%:@@%:@ m4_bpatsubst([$1], [.], m4_if([$2], [], [[-]], [[$2]])) @%:@@%:@[]dnl
])


# m4_qlen(STRING)
# ---------------
# Expands to the length of STRING after autom4te converts all quadrigraphs.
m4_define([m4_qlen],
[m4_len(m4_bpatsubsts([[$1]], [@\(<:\|:>\|S|\|%:\)@], [P], [@&t@]))])


# m4_qdelta(STRING)
# -----------------
# Expands to the net change in the length of STRING from autom4te converting the
# quadrigraphs in STRING.  This number is always negative or zero.
m4_define([m4_qdelta],
[m4_eval(m4_qlen([$1]) - m4_len([$1]))])



## ----------------------- ##
## 10. Number processing.  ##
## ----------------------- ##

# m4_sign(A)
# ----------
#
# The sign of the integer A.
m4_define([m4_sign],
[m4_bmatch([$1],
	   [^-], -1,
	   [^0+], 0,
		  1)])

# m4_cmp(A, B)
# ------------
#
# Compare two integers.
# A < B -> -1
# A = B ->  0
# A > B ->  1
m4_define([m4_cmp],
[m4_sign(m4_eval([$1 - $2]))])


# m4_list_cmp(A, B)
# -----------------
#
# Compare the two lists of integers A and B.  For instance:
#   m4_list_cmp((1, 0),     (1))    ->  0
#   m4_list_cmp((1, 0),     (1, 0)) ->  0
#   m4_list_cmp((1, 2),     (1, 0)) ->  1
#   m4_list_cmp((1, 2, 3),  (1, 2)) ->  1
#   m4_list_cmp((1, 2, -3), (1, 2)) -> -1
#   m4_list_cmp((1, 0),     (1, 2)) -> -1
#   m4_list_cmp((1),        (1, 2)) -> -1
m4_define([m4_list_cmp],
[m4_if([$1$2], [()()], 0,
       [$1], [()], [$0((0), [$2])],
       [$2], [()], [$0([$1], (0))],
       [m4_case(m4_cmp(m4_car$1, m4_car$2),
		-1, -1,
		 1, 1,
		 0, [$0((m4_shift$1), (m4_shift$2))])])])



## ------------------------ ##
## 11. Version processing.  ##
## ------------------------ ##


# m4_version_unletter(VERSION)
# ----------------------------
# Normalize beta version numbers with letters to numbers only for comparison.
#
#   Nl -> (N+1).-1.(l#)
#
#i.e., 2.14a -> 2.15.-1.1, 2.14b -> 2.15.-1.2, etc.
# This macro is absolutely not robust to active macro, it expects
# reasonable version numbers and is valid up to `z', no double letters.
m4_define([m4_version_unletter],
[m4_translit(m4_bpatsubsts([$1],
			   [\([0-9]+\)\([abcdefghi]\)],
			     [m4_eval(\1 + 1).-1.\2],
			   [\([0-9]+\)\([jklmnopqrs]\)],
			     [m4_eval(\1 + 1).-1.1\2],
			   [\([0-9]+\)\([tuvwxyz]\)],
			     [m4_eval(\1 + 1).-1.2\2]),
	     [abcdefghijklmnopqrstuvwxyz],
	     [12345678901234567890123456])])


# m4_version_compare(VERSION-1, VERSION-2)
# ----------------------------------------
# Compare the two version numbers and expand into
#  -1 if VERSION-1 < VERSION-2
#   0 if           =
#   1 if           >
m4_define([m4_version_compare],
[m4_list_cmp((m4_split(m4_version_unletter([$1]), [\.])),
	     (m4_split(m4_version_unletter([$2]), [\.])))])


# m4_PACKAGE_NAME
# m4_PACKAGE_TARNAME
# m4_PACKAGE_VERSION
# m4_PACKAGE_STRING
# m4_PACKAGE_BUGREPORT
# --------------------
#m4_include([m4sugar/version.m4]) # This is needed for Autoconf, but not Bison.


# m4_version_prereq(VERSION, [IF-OK], [IF-NOT = FAIL])
# ----------------------------------------------------
# Check this Autoconf version against VERSION.
m4_define([m4_version_prereq],
[m4_if(m4_version_compare(m4_defn([m4_PACKAGE_VERSION]), [$1]), -1,
       [m4_default([$3],
		   [m4_fatal([Autoconf version $1 or higher is required],
			     63)])],
       [$2])[]dnl
])



## ------------------- ##
## 12. File handling.  ##
## ------------------- ##


# It is a real pity that M4 comes with no macros to bind a diversion
# to a file.  So we have to deal without, which makes us a lot more
# fragile that we should.


# m4_file_append(FILE-NAME, CONTENT)
# ----------------------------------
m4_define([m4_file_append],
[m4_syscmd([cat >>$1 <<_m4eof
$2
_m4eof
])
m4_if(m4_sysval, [0], [],
      [m4_fatal([$0: cannot write: $1])])])



## ------------------------ ##
## 13. Setting M4sugar up.  ##
## ------------------------ ##


# m4_init
# -------
m4_define([m4_init],
[# All the M4sugar macros start with `m4_', except `dnl' kept as is
# for sake of simplicity.
m4_pattern_forbid([^_?m4_])
m4_pattern_forbid([^dnl$])

# Check the divert push/pop perfect balance.
m4_wrap([m4_ifdef([_m4_divert_diversion],
	   [m4_fatal([$0: unbalanced m4_divert_push:]_m4_divert_n_stack)])[]])

m4_divert_push([KILL])
m4_wrap([m4_divert_pop([KILL])[]])
])
