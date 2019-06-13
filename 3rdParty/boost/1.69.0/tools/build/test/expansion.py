#!/usr/bin/python

# Copyright 2003 Vladimir Prus
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)


import BoostBuild

t = BoostBuild.Tester(arguments=["--config="], pass_toolset=0)

t.write("source.input", "")

t.write("test-properties.jam", """
import feature : feature ;
import generators ;
import toolset ;
import type ;

# We're not using the toolset at all, and we want to
# suppress toolset initialization to avoid surprises.
feature.extend toolset : null ;

type.register CHECK : check ;
type.register INPUT : input ;
feature expected-define : : free ;
feature unexpected-define : : free ;
toolset.flags test-properties DEFINES : <define> ;
toolset.flags test-properties EXPECTED : <expected-define> ;
toolset.flags test-properties UNEXPECTED : <unexpected-define> ;
generators.register-standard test-properties.check : INPUT : CHECK ;
rule check ( target : source : properties * )
{
    local defines = [ on $(target) return $(DEFINES) ] ;
    for local macro in [ on $(target) return $(EXPECTED) ]
    {
        if ! ( $(macro) in $(defines) )
        {
            EXIT expected $(macro) for $(target) in $(properties) : 1 ;
        }
    }
    for local macro in [ on $(target) return $(UNEXPECTED) ]
    {
        if $(macro) in $(defines)
        {
            EXIT unexpected $(macro) for $(target) in $(properties) : 1 ;
        }
    }
}
actions check
{
    echo okay > $(<)
}
""")

t.write("jamfile.jam", """
import test-properties ;
# See if default value of composite feature 'cf' will be expanded to
# <define>CF_IS_OFF.
check a : source.input : <expected-define>CF_IS_OFF ;

# See if subfeature in requirements in expanded.
check b : source.input : <cf>on-1
    <expected-define>CF_1 <unexpected-define>CF_IS_OFF ;

# See if conditional requirements are recursively expanded.
check c : source.input : <toolset>null:<variant>release
    <variant>release:<define>FOO <expected-define>FOO
    ;

# Composites specified in the default build should not
# be expanded if they are overridden in the the requirements.
check d : source.input : <cf>on <unexpected-define>CF_IS_OFF : <cf>off ;

# Overriding a feature should clear subfeatures and
# apply default values of subfeatures.
check e : source.input : <cf>always
    <unexpected-define>CF_IS_OFF <expected-define>CF_2 <unexpected-define>CF_1
  : <cf>on-1  ;

# Subfeatures should not be changed if the parent feature doesn't change
check f : source.input : <cf>on <expected-define>CF_1 : <cf>on-1 ;

# If a subfeature is not specific to the value of the parent feature,
# then changing the parent value should not clear the subfeature.
check g : source.input : <fopt>off <expected-define>FOPT_2 : <fopt>on-2 ;

# If the default value of a composite feature adds an optional
# feature which has a subfeature with a default, then that
# default should be added.
check h : source.input : <expected-define>CX_2 ;

# If the default value of a feature is used, then the
# default value of its subfeatures should also be used.
check i : source.input : <expected-define>SF_1 ;
""")

t.write("jamroot.jam", """
import feature ;
feature.feature cf : off on always : composite incidental ;
feature.compose <cf>off : <define>CF_IS_OFF ;
feature.subfeature cf on : version : 1 2 : composite optional incidental ;
feature.compose <cf-on:version>1 : <define>CF_1 ;
feature.subfeature cf always : version : 1 2 : composite incidental ;
feature.compose <cf-always:version>1 : <define>CF_2 ;
feature.feature fopt : on off : optional incidental ;
feature.subfeature fopt : version : 1 2 : composite incidental ;
feature.compose <fopt-version>2 : <define>FOPT_2 ;

feature.feature cx1 : on : composite incidental ;
feature.feature cx2 : on : optional incidental ;
feature.subfeature cx2 on : sub : 1 : composite incidental ;
feature.compose <cx1>on : <cx2>on ;
feature.compose <cx2-on:sub>1 : <define>CX_2 ;

feature.feature sf : a : incidental ;
feature.subfeature sf a : sub : 1 : composite incidental ;
feature.compose <sf-a:sub>1 : <define>SF_1 ;
""")

t.expand_toolset("jamfile.jam")

t.run_build_system()
t.expect_addition(["bin/debug/a.check",
                   "bin/debug/b.check",
                   "bin/null/release/c.check",
                   "bin/debug/d.check",
                   "bin/debug/e.check",
                   "bin/debug/f.check",
                   "bin/debug/g.check",
                   "bin/debug/h.check",
                   "bin/debug/i.check"])

t.cleanup()
