#!/usr/bin/python

# Copyright 2018 Steven Watanabe
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

# Tests the <relevant> feature

import BoostBuild

t = BoostBuild.Tester(use_test_config=False)

t.write("xxx.jam", """
import type ;
import feature : feature ;
import toolset : flags ;
import generators ;
type.register XXX : xxx ;
type.register YYY : yyy ;
feature xxxflags : : free ;
generators.register-standard xxx.run : YYY : XXX ;
# xxxflags is relevant because it is used by flags
flags xxx.run OPTIONS : <xxxflags> ;
actions run
{
    echo okay > $(<)
}
""")

t.write("zzz.jam", """
import xxx ;
import type ;
import feature : feature ;
import generators ;
type.register ZZZ : zzz ;
feature zzz.enabled : off on : propagated ;
# zzz.enabled is relevant because it is used in the generator's
# requirements
generators.register-standard zzz.run : XXX : ZZZ : <zzz.enabled>on ;
actions run
{
    echo okay > $(<)
}
""")

t.write("aaa.jam", """
import zzz ;
import type ;
import feature : feature ;
import generators ;
import toolset : flags ;
type.register AAA : aaa ;
feature aaaflags : : free ;
generators.register-standard aaa.run : ZZZ : AAA ;
flags aaa.run OPTIONS : <aaaflags> ;
actions run
{
    echo okay > $(<)
}
""")

t.write("Jamroot.jam", """
import xxx ;
import zzz ;
import aaa ;
import feature : feature ;

# f1 is relevant, because it is composite and <xxxflags> is relevant
feature f1 : n y : composite propagated ;
feature.compose <f1>y : <xxxflags>-no1 ;
# f2 is relevant, because it is used in a conditional
feature f2 : n y : propagated ;
# f3 is relevant, because it is used to choose the target alternative
feature f3 : n y : propagated ;
# f4 is relevant, because it is marked as such explicitly
feature f4 : n y : propagated ;
# f5 is relevant because of the conditional usage-requirements
feature f5 : n y : propagated ;
# f6 is relevant because the indirect conditional indicates so
feature f6 : n y : propagated ;
# f7 is relevant because the icond7 says so
feature f7 : n y : propagated ;

# The same as f[n], except not propagated
feature g1 : n y : composite ;
feature.compose <g1>y : <xxxflags>-no1 ;
feature g2 : n y ;
feature g3 : n y ;
feature g4 : n y ;
feature g5 : n y ;
feature g6 : n y ;
feature g7 : n y ;

project : default-build
    <f1>y <f2>y <f3>y <f4>y <f5>y <f6>y <f7>y
    <g1>y <g2>y <g3>y <g4>y <g5>y <g6>y <g7>y <zzz.enabled>on ;

rule icond6 ( properties * )
{
    local result ;
    if <f6>y in $(properties) || <g6>y in $(properties)
    {
        result += <xxxflags>-yes6 ;
    }
    return $(result)
        <relevant>xxxflags:<relevant>f6
        <relevant>xxxflags:<relevant>g6 ;
}

rule icond7 ( properties * )
{
    local result ;
    if <f7>y in $(properties) || <g7>y in $(properties)
    {
        result += <aaaflags>-yes7 ;
    }
    return $(result)
        <relevant>aaaflags:<relevant>f7
        <relevant>aaaflags:<relevant>g7 ;
}

zzz out : in.yyy
  : <f2>y:<xxxflags>-no2 <g2>y:<xxxflags>-no2 <relevant>f4 <relevant>g4
    <conditional>@icond6
  :
  : <f5>y:<aaaflags>-yes5 <g5>y:<aaaflags>-yes5 <conditional>@icond7
  ;
alias out : : <f3>n ;
alias out : : <g3>n ;
# Features that are relevant for out are also relevant for check-propagate
aaa check-propagate : out ;
""")

t.write("in.yyy", "")

t.run_build_system()
t.expect_addition("bin/f1-y/f2-y/f3-y/f4-y/f6-y/g1-y/g2-y/g3-y/g4-y/g6-y/out.xxx")
t.expect_addition("bin/f1-y/f2-y/f3-y/f4-y/f6-y/g1-y/g2-y/g3-y/g4-y/g6-y/zzz.enabled-on/out.zzz")
t.expect_addition("bin/f1-y/f2-y/f3-y/f4-y/f5-y/f6-y/f7-y/zzz.enabled-on/check-propagate.aaa")

t.cleanup()
