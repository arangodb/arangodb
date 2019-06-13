#!/usr/bin/python

# Copyright (c) Steven Watanabe 2018.
# Distributed under the Boost Software License, Version 1.0. (See
# accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

# Tests that a single main target can be used for
# implicit dependencies of multiple different types.

import BoostBuild

t = BoostBuild.Tester(pass_toolset=False)

t.write("input.sss", "")

t.write("Jamroot.jam", """
import type ;
import common ;
import generators ;
import "class" : new ;
import feature : feature ;
import toolset : flags ;

type.register AAA : aaa ;
type.register BBB : bbb ;
type.register CCC : ccc ;
type.register DDD : ddd ;
type.register SSS : sss ;

feature aaa-path : : free path ;
feature bbb-path : : free path ;

class aaa-action : action
{
    rule adjust-properties ( property-set )
    {
        local s = [ $(self.targets[1]).creating-subvariant ] ;
        return [ $(property-set).add-raw
            [ $(s).implicit-includes aaa-path : AAA ] ] ;
    }
}

class aaa-generator : generator
{
    rule action-class ( )
    {
        return aaa-action ;
    }
}

class bbb-action : action
{
    rule adjust-properties ( property-set )
    {
        local s = [ $(self.targets[1]).creating-subvariant ] ;
        return [ $(property-set).add-raw
            [ $(s).implicit-includes bbb-path : BBB ] ] ;
    }
}

class bbb-generator : generator
{
    rule action-class ( )
    {
        return bbb-action ;
    }
}

generators.register-standard common.copy : SSS : AAA ;
generators.register-standard common.copy : SSS : BBB ;

# Produce two targets from a single source
rule make-aaa-bbb ( project name ? : property-set : sources * )
{
    local result ;
    local aaa = [ generators.construct $(project) $(name) : AAA :
        [ $(property-set).add-raw <location-prefix>a-loc ] : $(sources) ] ;
    local bbb = [ generators.construct $(project) $(name) : BBB :
        [ $(property-set).add-raw <location-prefix>b-loc ] : $(sources) ] ;
    return [ $(aaa[1]).add $(bbb[1]) ] $(aaa[2-]) $(bbb[2-]) ;
}

generate input : input.sss : <generating-rule>@make-aaa-bbb ;
explicit input ;

flags make-ccc AAAPATH : <aaa-path> ;
rule make-ccc ( target : sources * : properties * )
{
    ECHO aaa path\: [ on $(target) return $(AAAPATH) ] ;
    common.copy $(target) : $(sources) ;
}

flags make-ddd BBBPATH : <bbb-path> ;
rule make-ddd ( target : sources * : properties * )
{
    ECHO bbb path\: [ on $(target) return $(BBBPATH) ] ;
    common.copy $(target) : $(sources) ;
}

generators.register [ new aaa-generator $(__name__).make-ccc : SSS : CCC ] ;
generators.register [ new bbb-generator $(__name__).make-ddd : SSS : DDD ] ;

# This should have <aaapath>bin/a-loc
ccc output-c : input.sss : <implicit-dependency>input ;
# This should have <bbbpath>bin/b-loc
ddd output-d : input.sss : <implicit-dependency>input ;
""")

t.run_build_system()
t.expect_output_lines(["aaa path: bin/a-loc", "bbb path: bin/b-loc"])

t.cleanup()
