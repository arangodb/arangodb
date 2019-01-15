# Status: being ported by Vladimir Prus
# Base revision:  40958
#
# Copyright 2003 Dave Abrahams
# Copyright 2005 Rene Rivera
# Copyright 2002, 2003, 2004, 2005, 2006 Vladimir Prus
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)

""" Support for toolset definition.
"""
import sys

import feature, property, generators, property_set
import b2.util.set
import bjam

from b2.util import cached, qualify_jam_action, is_iterable_typed, is_iterable
from b2.util.utility import *
from b2.util import bjam_signature, sequence
from b2.manager import get_manager

__re_split_last_segment = re.compile (r'^(.+)\.([^\.])*')
__re_two_ampersands = re.compile ('(&&)')
__re_first_segment = re.compile ('([^.]*).*')
__re_first_group = re.compile (r'[^.]*\.(.*)')
_ignore_toolset_requirements = '--ignore-toolset-requirements' not in sys.argv

# Flag is a mechanism to set a value
# A single toolset flag. Specifies that when certain
# properties are in build property set, certain values
# should be appended to some variable.
#
# A flag applies to a specific action in specific module.
# The list of all flags for a module is stored, and each
# flag further contains the name of the rule it applies
# for,
class Flag:

    def __init__(self, variable_name, values, condition, rule = None):
        assert isinstance(variable_name, basestring)
        assert is_iterable(values) and all(
            isinstance(v, (basestring, type(None))) for v in values)
        assert is_iterable_typed(condition, property_set.PropertySet)
        assert isinstance(rule, (basestring, type(None)))
        self.variable_name = variable_name
        self.values = values
        self.condition = condition
        self.rule = rule

    def __str__(self):
        return("Flag(" + str(self.variable_name) + ", " + str(self.values) +\
               ", " + str(self.condition) + ", " + str(self.rule) + ")")

def reset ():
    """ Clear the module state. This is mainly for testing purposes.
    """
    global __module_flags, __flags, __stv

    # Mapping from module name to a list of all flags that apply
    # to either that module directly, or to any rule in that module.
    # Each element of the list is Flag instance.
    # So, for module named xxx this might contain flags for 'xxx',
    # for 'xxx.compile', for 'xxx.compile.c++', etc.
    __module_flags = {}

    # Mapping from specific rule or module name to a list of Flag instances
    # that apply to that name.
    # Say, it might contain flags for 'xxx.compile.c++'. If there are
    # entries for module name 'xxx', they are flags for 'xxx' itself,
    # not including any rules in that module.
    __flags = {}

    # A cache for variable settings. The key is generated from the rule name and the properties.
    __stv = {}

reset ()

# FIXME: --ignore-toolset-requirements
def using(toolset_module, *args):
    if isinstance(toolset_module, (list, tuple)):
        toolset_module = toolset_module[0]
    loaded_toolset_module= get_manager().projects().load_module(toolset_module, [os.getcwd()]);
    loaded_toolset_module.init(*args)

# FIXME push-checking-for-flags-module ....
# FIXME: investigate existing uses of 'hack-hack' parameter
# in jam code.

@bjam_signature((["rule_or_module", "variable_name", "condition", "*"],
                 ["values", "*"]))
def flags(rule_or_module, variable_name, condition, values = []):
    """ Specifies the flags (variables) that must be set on targets under certain
        conditions, described by arguments.
        rule_or_module:   If contains dot, should be a rule name.
                          The flags will be applied when that rule is
                          used to set up build actions.

                          If does not contain dot, should be a module name.
                          The flags will be applied for all rules in that
                          module.
                          If module for rule is different from the calling
                          module, an error is issued.

         variable_name:   Variable that should be set on target

         condition        A condition when this flag should be applied.
                          Should be set of property sets. If one of
                          those property sets is contained in build
                          properties, the flag will be used.
                          Implied values are not allowed:
                          "<toolset>gcc" should be used, not just
                          "gcc". Subfeatures, like in "<toolset>gcc-3.2"
                          are allowed. If left empty, the flag will
                          always used.

                          Property sets may use value-less properties
                          ('<a>'  vs. '<a>value') to match absent
                          properties. This allows to separately match

                             <architecture>/<address-model>64
                             <architecture>ia64/<address-model>

                          Where both features are optional. Without this
                          syntax we'd be forced to define "default" value.

         values:          The value to add to variable. If <feature>
                          is specified, then the value of 'feature'
                          will be added.
    """
    assert isinstance(rule_or_module, basestring)
    assert isinstance(variable_name, basestring)
    assert is_iterable_typed(condition, basestring)
    assert is_iterable(values) and all(isinstance(v, (basestring, type(None))) for v in values)
    caller = bjam.caller()
    if not '.' in rule_or_module and caller and caller[:-1].startswith("Jamfile"):
        # Unqualified rule name, used inside Jamfile. Most likely used with
        # 'make' or 'notfile' rules. This prevents setting flags on the entire
        # Jamfile module (this will be considered as rule), but who cares?
        # Probably, 'flags' rule should be split into 'flags' and
        # 'flags-on-module'.
        rule_or_module = qualify_jam_action(rule_or_module, caller)
    else:
        # FIXME: revive checking that we don't set flags for a different
        # module unintentionally
        pass

    if condition and not replace_grist (condition, ''):
        # We have condition in the form '<feature>', that is, without
        # value. That's a previous syntax:
        #
        #   flags gcc.link RPATH <dll-path> ;
        # for compatibility, convert it to
        #   flags gcc.link RPATH : <dll-path> ;
        values = [ condition ]
        condition = None

    if condition:
        transformed = []
        for c in condition:
            # FIXME: 'split' might be a too raw tool here.
            pl = [property.create_from_string(s,False,True) for s in c.split('/')]
            pl = feature.expand_subfeatures(pl);
            transformed.append(property_set.create(pl))
        condition = transformed

        property.validate_property_sets(condition)

    __add_flag (rule_or_module, variable_name, condition, values)

def set_target_variables (manager, rule_or_module, targets, ps):
    """
    """
    assert isinstance(rule_or_module, basestring)
    assert is_iterable_typed(targets, basestring)
    assert isinstance(ps, property_set.PropertySet)
    settings = __set_target_variables_aux(manager, rule_or_module, ps)

    if settings:
        for s in settings:
            for target in targets:
                manager.engine ().set_target_variable (target, s [0], s[1], True)

def find_satisfied_condition(conditions, ps):
    """Returns the first element of 'property-sets' which is a subset of
    'properties', or an empty list if no such element exists."""
    assert is_iterable_typed(conditions, property_set.PropertySet)
    assert isinstance(ps, property_set.PropertySet)

    for condition in conditions:

        found_all = True
        for i in condition.all():

            if i.value:
                found = i.value in ps.get(i.feature)
            else:
                # Handle value-less properties like '<architecture>' (compare with
                # '<architecture>x86').
                # If $(i) is a value-less property it should match default
                # value of an optional property. See the first line in the
                # example below:
                #
                #  property set     properties     result
                # <a> <b>foo      <b>foo           match
                # <a> <b>foo      <a>foo <b>foo    no match
                # <a>foo <b>foo   <b>foo           no match
                # <a>foo <b>foo   <a>foo <b>foo    match
                found = not ps.get(i.feature)

            found_all = found_all and found

        if found_all:
            return condition

    return None


def register (toolset):
    """ Registers a new toolset.
    """
    assert isinstance(toolset, basestring)
    feature.extend('toolset', [toolset])

def inherit_generators (toolset, properties, base, generators_to_ignore = []):
    assert isinstance(toolset, basestring)
    assert is_iterable_typed(properties, basestring)
    assert isinstance(base, basestring)
    assert is_iterable_typed(generators_to_ignore, basestring)
    if not properties:
        properties = [replace_grist (toolset, '<toolset>')]

    base_generators = generators.generators_for_toolset(base)

    for g in base_generators:
        id = g.id()

        if not id in generators_to_ignore:
            # Some generator names have multiple periods in their name, so
            # $(id:B=$(toolset)) doesn't generate the right new_id name.
            # e.g. if id = gcc.compile.c++, $(id:B=darwin) = darwin.c++,
            # which is not what we want. Manually parse the base and suffix
            # (if there's a better way to do this, I'd love to see it.)
            # See also register in module generators.
            (base, suffix) = split_action_id(id)

            new_id = toolset + '.' + suffix

            generators.register(g.clone(new_id, properties))

def inherit_flags(toolset, base, prohibited_properties = []):
    """Brings all flag definitions from the 'base' toolset into the 'toolset'
    toolset. Flag definitions whose conditions make use of properties in
    'prohibited-properties' are ignored. Don't confuse property and feature, for
    example <debug-symbols>on and <debug-symbols>off, so blocking one of them does
    not block the other one.

    The flag conditions are not altered at all, so if a condition includes a name,
    or version of a base toolset, it won't ever match the inheriting toolset. When
    such flag settings must be inherited, define a rule in base toolset module and
    call it as needed."""
    assert isinstance(toolset, basestring)
    assert isinstance(base, basestring)
    assert is_iterable_typed(prohibited_properties, basestring)
    for f in __module_flags.get(base, []):

        if not f.condition or b2.util.set.difference(f.condition, prohibited_properties):
            match = __re_first_group.match(f.rule)
            rule_ = None
            if match:
                rule_ = match.group(1)

            new_rule_or_module = ''

            if rule_:
                new_rule_or_module = toolset + '.' + rule_
            else:
                new_rule_or_module = toolset

            __add_flag (new_rule_or_module, f.variable_name, f.condition, f.values)


def inherit_rules(toolset, base):
    engine = get_manager().engine()
    new_actions = {}
    for action_name, action in engine.actions.iteritems():
        module, id = split_action_id(action_name)
        if module == base:
            new_action_name = toolset + '.' + id
            # make sure not to override any existing actions
            # that may have been declared already
            if new_action_name not in engine.actions:
                new_actions[new_action_name] = action

    engine.actions.update(new_actions)

######################################################################################
# Private functions

@cached
def __set_target_variables_aux (manager, rule_or_module, ps):
    """ Given a rule name and a property set, returns a list of tuples of
        variables names and values, which must be set on targets for that
        rule/properties combination.
    """
    assert isinstance(rule_or_module, basestring)
    assert isinstance(ps, property_set.PropertySet)
    result = []

    for f in __flags.get(rule_or_module, []):

        if not f.condition or find_satisfied_condition (f.condition, ps):
            processed = []
            for v in f.values:
                # The value might be <feature-name> so needs special
                # treatment.
                processed += __handle_flag_value (manager, v, ps)

            for r in processed:
                result.append ((f.variable_name, r))

    # strip away last dot separated part and recurse.
    next = __re_split_last_segment.match(rule_or_module)

    if next:
        result.extend(__set_target_variables_aux(
            manager, next.group(1), ps))

    return result

def __handle_flag_value (manager, value, ps):
    assert isinstance(value, basestring)
    assert isinstance(ps, property_set.PropertySet)
    result = []

    if get_grist (value):
        f = feature.get(value)
        values = ps.get(f)

        for value in values:

            if f.dependency:
                # the value of a dependency feature is a target
                # and must be actualized
                result.append(value.actualize())

            elif f.path or f.free:

                # Treat features with && in the value
                # specially -- each &&-separated element is considered
                # separate value. This is needed to handle searched
                # libraries, which must be in specific order.
                if not __re_two_ampersands.search(value):
                    result.append(value)

                else:
                    result.extend(value.split ('&&'))
            else:
                result.append (value)
    else:
        result.append (value)

    return sequence.unique(result, stable=True)

def __add_flag (rule_or_module, variable_name, condition, values):
    """ Adds a new flag setting with the specified values.
        Does no checking.
    """
    assert isinstance(rule_or_module, basestring)
    assert isinstance(variable_name, basestring)
    assert is_iterable_typed(condition, property_set.PropertySet)
    assert is_iterable(values) and all(
        isinstance(v, (basestring, type(None))) for v in values)
    f = Flag(variable_name, values, condition, rule_or_module)

    # Grab the name of the module
    m = __re_first_segment.match (rule_or_module)
    assert m
    module = m.group(1)

    __module_flags.setdefault(module, []).append(f)
    __flags.setdefault(rule_or_module, []).append(f)

__requirements = []

def requirements():
    """Return the list of global 'toolset requirements'.
    Those requirements will be automatically added to the requirements of any main target."""
    return __requirements

def add_requirements(requirements):
    """Adds elements to the list of global 'toolset requirements'. The requirements
    will be automatically added to the requirements for all main targets, as if
    they were specified literally. For best results, all requirements added should
    be conditional or indirect conditional."""
    assert is_iterable_typed(requirements, basestring)

    if _ignore_toolset_requirements:
        __requirements.extend(requirements)


# Make toolset 'toolset', defined in a module of the same name,
# inherit from 'base'
# 1. The 'init' rule from 'base' is imported into 'toolset' with full
#    name. Another 'init' is called, which forwards to the base one.
# 2. All generators from 'base' are cloned. The ids are adjusted and
#    <toolset> property in requires is adjusted too
# 3. All flags are inherited
# 4. All rules are imported.
def inherit(toolset, base):
    assert isinstance(toolset, basestring)
    assert isinstance(base, basestring)
    get_manager().projects().load_module(base, ['.']);

    inherit_generators(toolset, [], base)
    inherit_flags(toolset, base)
    inherit_rules(toolset, base)
