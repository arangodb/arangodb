# Copyright Craig Rodrigues 2005.
# Copyright (c) 2008 Steven Watanabe
#
# Distributed under the Boost
# Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
from b2.build import type as type_
from b2.manager import get_manager
from b2.tools.cast import cast
from b2.util import bjam_signature


MANAGER = get_manager()
PROJECT_REGISTRY = MANAGER.projects()

# maps project.name() + type to type
_project_types = {}

type_.register_type('ASM', ['s', 'S', 'asm'])


@bjam_signature((['type_'], ['sources', '*'], ['name', '?']))
def set_asm_type(type_, sources, name=''):
    project = PROJECT_REGISTRY.current()
    _project_types[project.name() + type_] = _project_types.get(
        project.name() + type_, type_) + '_'

    name = name if name else _project_types[project.name() + type_]
    type_ += '.asm'
    cast(name, type_.upper(), sources, [], [], [])


PROJECT_REGISTRY.add_rule("set-asm-type", set_asm_type)
