# Status: ported.
# Base revison: 64488
#
# Copyright (c) 2010 Vladimir Prus.
#
# Use, modification and distribution is subject to the Boost Software
# License Version 1.0. (See accompanying file LICENSE_1_0.txt or
# http://www.boost.org/LICENSE_1_0.txt)

# This module defines function to help with two main tasks:
#
# - Discovering build-time configuration for the purposes of adjusting
#   build process.
# - Reporting what is built, and how it is configured.

import b2.build.property as property
import b2.build.property_set as property_set

from b2.build import targets as targets_

from b2.manager import get_manager
from b2.util.sequence import unique
from b2.util import bjam_signature, value_to_jam, is_iterable

import bjam
import os

__width = 30

def set_width(width):
    global __width
    __width = 30

__components = []
__built_components = []
__component_logs = {}
__announced_checks = False

__log_file = None
__log_fd = -1

def register_components(components):
    """Declare that the components specified by the parameter exist."""
    assert is_iterable(components)
    __components.extend(components)

def components_building(components):
    """Declare that the components specified by the parameters will be build."""
    assert is_iterable(components)
    __built_components.extend(components)

def log_component_configuration(component, message):
    """Report something about component configuration that the user should better know."""
    assert isinstance(component, basestring)
    assert isinstance(message, basestring)
    __component_logs.setdefault(component, []).append(message)

def log_check_result(result):
    assert isinstance(result, basestring)
    global __announced_checks
    if not __announced_checks:
        print "Performing configuration checks"
        __announced_checks = True

    print result

def log_library_search_result(library, result):
    assert isinstance(library, basestring)
    assert isinstance(result, basestring)
    log_check_result(("    - %(library)s : %(result)s" % locals()).rjust(__width))


def print_component_configuration():

    print "\nComponent configuration:"
    for c in __components:
        if c in __built_components:
            s = "building"
        else:
            s = "not building"
        message = "    - %s)" % c
        message = message.rjust(__width)
        message += " : " + s
        for m in __component_logs.get(c, []):
            print "        -" + m
    print ""

__builds_cache = {}

def builds(metatarget_reference, project, ps, what):
    # Attempt to build a metatarget named by 'metatarget-reference'
    # in context of 'project' with properties 'ps'.
    # Returns non-empty value if build is OK.
    assert isinstance(metatarget_reference, basestring)
    assert isinstance(project, targets_.ProjectTarget)
    assert isinstance(ps, property_set.PropertySet)
    assert isinstance(what, basestring)

    result = []

    existing = __builds_cache.get((what, ps), None)
    if existing is None:

        result = False
        __builds_cache[(what, ps)] = False

        targets = targets_.generate_from_reference(
            metatarget_reference, project, ps).targets()
        jam_targets = []
        for t in targets:
            jam_targets.append(t.actualize())

        x = ("    - %s" % what).rjust(__width)
        if bjam.call("UPDATE_NOW", jam_targets, str(__log_fd), "ignore-minus-n"):
            __builds_cache[(what, ps)] = True
            result = True
            log_check_result("%s: yes" % x)
        else:
            log_check_result("%s: no" % x)

        return result
    else:
        return existing

def set_log_file(log_file_name):
    assert isinstance(log_file_name, basestring)
    # Called by Boost.Build startup code to specify name of a file
    # that will receive results of configure checks.  This
    # should never be called by users.
    global __log_file, __log_fd
    dirname = os.path.dirname(log_file_name)
    if not os.path.exists(dirname):
        os.makedirs(dirname)
    # Make sure to keep the file around, so that it's not
    # garbage-collected and closed
    __log_file = open(log_file_name, "w")
    __log_fd = __log_file.fileno()

# Frontend rules

class CheckTargetBuildsWorker:

    def __init__(self, target, true_properties, false_properties):
        self.target = target
        self.true_properties = property.create_from_strings(true_properties, True)
        self.false_properties = property.create_from_strings(false_properties, True)

    def check(self, ps):
        assert isinstance(ps, property_set.PropertySet)
        # FIXME: this should not be hardcoded. Other checks might
        # want to consider different set of features as relevant.
        toolset = ps.get('toolset')[0]
        toolset_version_property = "<toolset-" + toolset + ":version>" ;
        relevant = ps.get_properties('target-os') + \
                   ps.get_properties("toolset") + \
                   ps.get_properties(toolset_version_property) + \
                   ps.get_properties("address-model") + \
                   ps.get_properties("architecture")
        rps = property_set.create(relevant)
        t = get_manager().targets().current()
        p = t.project()
        if builds(self.target, p, rps, "%s builds" % self.target):
            choosen = self.true_properties
        else:
            choosen = self.false_properties
        return property.evaluate_conditionals_in_context(choosen, ps)

@bjam_signature((["target"], ["true_properties", "*"], ["false_properties", "*"]))
def check_target_builds(target, true_properties, false_properties):
    worker = CheckTargetBuildsWorker(target, true_properties, false_properties)
    value = value_to_jam(worker.check)
    return "<conditional>" + value

get_manager().projects().add_rule("check-target-builds", check_target_builds)


