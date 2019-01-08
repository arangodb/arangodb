#!/usr/bin/python

# Copyright 2014 Steven Watanabe
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)

# Test the handling of toolset.add-requirements

import BoostBuild

t = BoostBuild.Tester(pass_toolset=0, ignore_toolset_requirements=False)

t.write('jamroot.jam', '''
import toolset ;
import errors ;

rule test-rule ( properties * )
{
  return <define>TEST_INDIRECT_CONDITIONAL ;
}

toolset.add-requirements
  <define>TEST_MACRO
  <conditional>@test-rule
  <link>shared:<define>TEST_CONDITIONAL
;

rule check-requirements ( target : sources * : properties * )
{
  local macros = TEST_MACRO TEST_CONDITIONAL TEST_INDIRECT_CONDITIONAL ;
  for local m in $(macros)
  {
    if ! <define>$(m) in $(properties)
    {
      errors.error $(m) not defined ;
    }
  }
}
make test : : @check-requirements ;
''')

t.run_build_system()

t.cleanup()
