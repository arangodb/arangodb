#!/usr/bin/python

# Copyright 2018 Steven Watanabe
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)

# Test the handling of toolset.add-defaults

import BoostBuild

t = BoostBuild.Tester(pass_toolset=0, ignore_toolset_requirements=False)

t.write('jamroot.jam', '''
import toolset ;
import errors ;
import feature : feature ;
import set ;

feature f1 : a b ;
feature f2 : c d ;
feature f3 : e f ;
feature f4 : g h ;
feature f5 : i j ;
feature f6 : k l m ;

rule test-rule ( properties * )
{
  if <f1>a in $(properties)
  {
    return <f2>d ;
  }
}

toolset.add-defaults
  <conditional>@test-rule
  <f3>e:<f4>h
  <f5>i:<f6>l
;

rule check-requirements ( target : sources * : properties * )
{
  local expected = <f2>d <f4>h <f6>m ;
  local unexpected = <f2>c <f4>g <f6>k <f6>l ;
  local missing = [ set.difference $(expected) : $(properties) ] ;
  if $(missing)
  {
    errors.error $(missing) not present ;
  }
  local extra = [ set.intersection $(unexpected) : $(properties) ] ;
  if $(extra)
  {
    errors.error $(extra) present ;
  }
}
make test : : @check-requirements : <f6>m ;
''')

t.run_build_system()

t.cleanup()
