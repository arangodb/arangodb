# Status: ported, except for tests.
# Base revision: 64070
#
# Copyright 2001, 2002, 2003 Dave Abrahams
# Copyright 2006 Rene Rivera
# Copyright 2002, 2003, 2004, 2005, 2006 Vladimir Prus
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)

import re
import sys
from functools import total_ordering

from b2.util.utility import *
from b2.build import feature
from b2.util import sequence, qualify_jam_action, is_iterable_typed
import b2.util.set
from b2.manager import get_manager


__re_two_ampersands = re.compile ('&&')
__re_comma = re.compile (',')
__re_split_condition = re.compile ('(.*):(<.*)')
__re_split_conditional = re.compile (r'(.+):<(.+)')
__re_colon = re.compile (':')
__re_has_condition = re.compile (r':<')
__re_separate_condition_and_property = re.compile (r'(.*):(<.*)')

_not_applicable_feature='not-applicable-in-this-context'
feature.feature(_not_applicable_feature, [], ['free'])

__abbreviated_paths = False


class PropertyMeta(type):
    """
    This class exists to implement the isinstance() and issubclass()
    hooks for the Property class. Since we've introduce the concept of
    a LazyProperty, isinstance(p, Property) will fail when p is a LazyProperty.
    Implementing both __instancecheck__ and __subclasscheck__ will allow
    LazyProperty instances to pass the isinstance() and issubclass check for
    the Property class.

    Additionally, the __call__ method intercepts the call to the Property
    constructor to ensure that calling Property with the same arguments
    will always return the same Property instance.
    """
    _registry = {}
    current_id = 1

    def __call__(mcs, f, value, condition=None):
        """
        This intercepts the call to the Property() constructor.

        This exists so that the same arguments will always return the same Property
        instance. This allows us to give each instance a unique ID.
        """
        from b2.build.feature import Feature
        if not isinstance(f, Feature):
            f = feature.get(f)
        if condition is None:
            condition = []
        key = (f, value) + tuple(sorted(condition))
        if key not in mcs._registry:
            instance = super(PropertyMeta, mcs).__call__(f, value, condition)
            mcs._registry[key] = instance
        return mcs._registry[key]

    @staticmethod
    def check(obj):
        return (hasattr(obj, 'feature') and
                hasattr(obj, 'value') and
                hasattr(obj, 'condition'))

    def __instancecheck__(self, instance):
        return self.check(instance)

    def __subclasscheck__(self, subclass):
        return self.check(subclass)


@total_ordering
class Property(object):

    __slots__ = ('feature', 'value', 'condition', '_to_raw', '_hash', 'id')
    __metaclass__ = PropertyMeta

    def __init__(self, f, value, condition=None):
        assert(f.free or ':' not in value)
        if condition is None:
            condition = []

        self.feature = f
        self.value = value
        self.condition = condition
        self._hash = hash((self.feature, self.value) + tuple(sorted(self.condition)))
        self.id = PropertyMeta.current_id
        # increment the id counter.
        # this allows us to take a list of Property
        # instances and use their unique integer ID
        # to create a key for PropertySet caching. This is
        # much faster than string comparison.
        PropertyMeta.current_id += 1

        condition_str = ''
        if condition:
            condition_str = ",".join(str(p) for p in self.condition) + ':'

        self._to_raw = '{}<{}>{}'.format(condition_str, f.name, value)

    def to_raw(self):
        return self._to_raw

    def __str__(self):

        return self._to_raw

    def __hash__(self):
        return self._hash

    def __eq__(self, other):
        return self._hash == other._hash

    def __lt__(self, other):
        return (self.feature.name, self.value) < (other.feature.name, other.value)


@total_ordering
class LazyProperty(object):
    def __init__(self, feature_name, value, condition=None):
        if condition is None:
            condition = []

        self.__property = Property(
            feature.get(_not_applicable_feature), feature_name + value, condition=condition)
        self.__name = feature_name
        self.__value = value
        self.__condition = condition
        self.__feature = None

    def __getattr__(self, item):
        if self.__feature is None:
            try:
                self.__feature = feature.get(self.__name)
                self.__property = Property(self.__feature, self.__value, self.__condition)
            except KeyError:
                pass
        return getattr(self.__property, item)

    def __hash__(self):
        return hash(self.__property)

    def __str__(self):
        return self.__property._to_raw

    def __eq__(self, other):
        return self.__property == other

    def __lt__(self, other):
        return (self.feature.name, self.value) < (other.feature.name, other.value)


def create_from_string(s, allow_condition=False,allow_missing_value=False):
    assert isinstance(s, basestring)
    assert isinstance(allow_condition, bool)
    assert isinstance(allow_missing_value, bool)
    condition = []
    import types
    if not isinstance(s, types.StringType):
        print type(s)
    if __re_has_condition.search(s):

        if not allow_condition:
            raise BaseException("Conditional property is not allowed in this context")

        m = __re_separate_condition_and_property.match(s)
        condition = m.group(1)
        s = m.group(2)

    # FIXME: break dependency cycle
    from b2.manager import get_manager

    if condition:
        condition = [create_from_string(x) for x in condition.split(',')]

    feature_name = get_grist(s)
    if not feature_name:
        if feature.is_implicit_value(s):
            f = feature.implied_feature(s)
            value = s
            p = Property(f, value, condition=condition)
        else:
            raise get_manager().errors()("Invalid property '%s' -- unknown feature" % s)
    else:
        value = get_value(s)
        if not value and not allow_missing_value:
            get_manager().errors()("Invalid property '%s' -- no value specified" % s)

        if feature.valid(feature_name):
            p = Property(feature.get(feature_name), value, condition=condition)
        else:
            # In case feature name is not known, it is wrong to do a hard error.
            # Feature sets change depending on the toolset. So e.g.
            # <toolset-X:version> is an unknown feature when using toolset Y.
            #
            # Ideally we would like to ignore this value, but most of
            # Boost.Build code expects that we return a valid Property. For this
            # reason we use a sentinel <not-applicable-in-this-context> feature.
            #
            # The underlying cause for this problem is that python port Property
            # is more strict than its Jam counterpart and must always reference
            # a valid feature.
            p = LazyProperty(feature_name, value, condition=condition)

    return p

def create_from_strings(string_list, allow_condition=False):
    assert is_iterable_typed(string_list, basestring)
    return [create_from_string(s, allow_condition) for s in string_list]

def reset ():
    """ Clear the module state. This is mainly for testing purposes.
    """
    global __results

    # A cache of results from as_path
    __results = {}

reset ()


def set_abbreviated_paths(on=True):
    global __abbreviated_paths
    if on == 'off':
        on = False
    on = bool(on)
    __abbreviated_paths = on


def get_abbreviated_paths():
    return __abbreviated_paths or '--abbreviated-paths' in sys.argv


def path_order (x, y):
    """ Helper for as_path, below. Orders properties with the implicit ones
        first, and within the two sections in alphabetical order of feature
        name.
    """
    if x == y:
        return 0

    xg = get_grist (x)
    yg = get_grist (y)

    if yg and not xg:
        return -1

    elif xg and not yg:
        return 1

    else:
        if not xg:
            x = feature.expand_subfeatures([x])
            y = feature.expand_subfeatures([y])

        if x < y:
            return -1
        elif x > y:
            return 1
        else:
            return 0

def identify(string):
    return string

# Uses Property
def refine (properties, requirements):
    """ Refines 'properties' by overriding any non-free properties
        for which a different value is specified in 'requirements'.
        Conditional requirements are just added without modification.
        Returns the resulting list of properties.
    """
    assert is_iterable_typed(properties, Property)
    assert is_iterable_typed(requirements, Property)
    # The result has no duplicates, so we store it in a set
    result = set()

    # Records all requirements.
    required = {}

    # All the elements of requirements should be present in the result
    # Record them so that we can handle 'properties'.
    for r in requirements:
        # Don't consider conditional requirements.
        if not r.condition:
            required[r.feature] = r

    for p in properties:
        # Skip conditional properties
        if p.condition:
            result.add(p)
        # No processing for free properties
        elif p.feature.free:
            result.add(p)
        else:
            if p.feature in required:
                result.add(required[p.feature])
            else:
                result.add(p)

    return sequence.unique(list(result) + requirements)

def translate_paths (properties, path):
    """ Interpret all path properties in 'properties' as relative to 'path'
        The property values are assumed to be in system-specific form, and
        will be translated into normalized form.
        """
    assert is_iterable_typed(properties, Property)
    result = []

    for p in properties:

        if p.feature.path:
            values = __re_two_ampersands.split(p.value)

            new_value = "&&".join(os.path.normpath(os.path.join(path, v)) for v in values)

            if new_value != p.value:
                result.append(Property(p.feature, new_value, p.condition))
            else:
                result.append(p)

        else:
            result.append (p)

    return result

def translate_indirect(properties, context_module):
    """Assumes that all feature values that start with '@' are
    names of rules, used in 'context-module'. Such rules can be
    either local to the module or global. Qualified local rules
    with the name of the module."""
    assert is_iterable_typed(properties, Property)
    assert isinstance(context_module, basestring)
    result = []
    for p in properties:
        if p.value[0] == '@':
            q = qualify_jam_action(p.value[1:], context_module)
            get_manager().engine().register_bjam_action(q)
            result.append(Property(p.feature, '@' + q, p.condition))
        else:
            result.append(p)

    return result

def validate (properties):
    """ Exit with error if any of the properties is not valid.
        properties may be a single property or a sequence of properties.
    """
    if isinstance(properties, Property):
        properties = [properties]
    assert is_iterable_typed(properties, Property)
    for p in properties:
        __validate1(p)

def expand_subfeatures_in_conditions (properties):
    assert is_iterable_typed(properties, Property)
    result = []
    for p in properties:

        if not p.condition:
            result.append(p)
        else:
            expanded = []
            for c in p.condition:
                # It common that condition includes a toolset which
                # was never defined, or mentiones subfeatures which
                # were never defined. In that case, validation will
                # only produce an spirious error, so don't validate.
                expanded.extend(feature.expand_subfeatures ([c], True))

            # we need to keep LazyProperties lazy
            if isinstance(p, LazyProperty):
                value = p.value
                feature_name = get_grist(value)
                value = value.replace(feature_name, '')
                result.append(LazyProperty(feature_name, value, condition=expanded))
            else:
                result.append(Property(p.feature, p.value, expanded))

    return result

# FIXME: this should go
def split_conditional (property):
    """ If 'property' is conditional property, returns
        condition and the property, e.g
        <variant>debug,<toolset>gcc:<inlining>full will become
        <variant>debug,<toolset>gcc <inlining>full.
        Otherwise, returns empty string.
    """
    assert isinstance(property, basestring)
    m = __re_split_conditional.match (property)

    if m:
        return (m.group (1), '<' + m.group (2))

    return None


def select (features, properties):
    """ Selects properties which correspond to any of the given features.
    """
    assert is_iterable_typed(properties, basestring)
    result = []

    # add any missing angle brackets
    features = add_grist (features)

    return [p for p in properties if get_grist(p) in features]

def validate_property_sets (sets):
    if __debug__:
        from .property_set import PropertySet
        assert is_iterable_typed(sets, PropertySet)
    for s in sets:
        validate(s.all())

def evaluate_conditionals_in_context (properties, context):
    """ Removes all conditional properties which conditions are not met
        For those with met conditions, removes the condition. Properties
        in conditions are looked up in 'context'
    """
    if __debug__:
        from .property_set import PropertySet
        assert is_iterable_typed(properties, Property)
        assert isinstance(context, PropertySet)
    base = []
    conditional = []

    for p in properties:
        if p.condition:
            conditional.append (p)
        else:
            base.append (p)

    result = base[:]
    for p in conditional:

        # Evaluate condition
        # FIXME: probably inefficient
        if all(x in context for x in p.condition):
            result.append(Property(p.feature, p.value))

    return result


def change (properties, feature, value = None):
    """ Returns a modified version of properties with all values of the
        given feature replaced by the given value.
        If 'value' is None the feature will be removed.
    """
    assert is_iterable_typed(properties, basestring)
    assert isinstance(feature, basestring)
    assert isinstance(value, (basestring, type(None)))
    result = []

    feature = add_grist (feature)

    for p in properties:
        if get_grist (p) == feature:
            if value:
                result.append (replace_grist (value, feature))

        else:
            result.append (p)

    return result


################################################################
# Private functions

def __validate1 (property):
    """ Exit with error if property is not valid.
    """
    assert isinstance(property, Property)
    msg = None

    if not property.feature.free:
        feature.validate_value_string (property.feature, property.value)


###################################################################
# Still to port.
# Original lines are prefixed with "#   "
#
#
#   import utility : ungrist ;
#   import sequence : unique ;
#   import errors : error ;
#   import feature ;
#   import regex ;
#   import sequence ;
#   import set ;
#   import path ;
#   import assert ;
#
#


#   rule validate-property-sets ( property-sets * )
#   {
#       for local s in $(property-sets)
#       {
#           validate [ feature.split $(s) ] ;
#       }
#   }
#

def remove(attributes, properties):
    """Returns a property sets which include all the elements
    in 'properties' that do not have attributes listed in 'attributes'."""
    if isinstance(attributes, basestring):
        attributes = [attributes]
    assert is_iterable_typed(attributes, basestring)
    assert is_iterable_typed(properties, basestring)
    result = []
    for e in properties:
        attributes_new = feature.attributes(get_grist(e))
        has_common_features = 0
        for a in attributes_new:
            if a in attributes:
                has_common_features = 1
                break

        if not has_common_features:
            result += e

    return result


def take(attributes, properties):
    """Returns a property set which include all
    properties in 'properties' that have any of 'attributes'."""
    assert is_iterable_typed(attributes, basestring)
    assert is_iterable_typed(properties, basestring)
    result = []
    for e in properties:
        if b2.util.set.intersection(attributes, feature.attributes(get_grist(e))):
            result.append(e)
    return result

def translate_dependencies(properties, project_id, location):
    assert is_iterable_typed(properties, Property)
    assert isinstance(project_id, basestring)
    assert isinstance(location, basestring)
    result = []
    for p in properties:

        if not p.feature.dependency:
            result.append(p)
        else:
            v = p.value
            m = re.match("(.*)//(.*)", v)
            if m:
                rooted = m.group(1)
                if rooted[0] == '/':
                    # Either project id or absolute Linux path, do nothing.
                    pass
                else:
                    rooted = os.path.join(os.getcwd(), location, rooted)

                result.append(Property(p.feature, rooted + "//" + m.group(2), p.condition))

            elif os.path.isabs(v):
                result.append(p)
            else:
                result.append(Property(p.feature, project_id + "//" + v, p.condition))

    return result


class PropertyMap:
    """ Class which maintains a property set -> string mapping.
    """
    def __init__ (self):
        self.__properties = []
        self.__values = []

    def insert (self, properties, value):
        """ Associate value with properties.
        """
        assert is_iterable_typed(properties, basestring)
        assert isinstance(value, basestring)
        self.__properties.append(properties)
        self.__values.append(value)

    def find (self, properties):
        """ Return the value associated with properties
        or any subset of it. If more than one
        subset has value assigned to it, return the
        value for the longest subset, if it's unique.
        """
        assert is_iterable_typed(properties, basestring)
        return self.find_replace (properties)

    def find_replace(self, properties, value=None):
        assert is_iterable_typed(properties, basestring)
        assert isinstance(value, (basestring, type(None)))
        matches = []
        match_ranks = []

        for i in range(0, len(self.__properties)):
            p = self.__properties[i]

            if b2.util.set.contains (p, properties):
                matches.append (i)
                match_ranks.append(len(p))

        best = sequence.select_highest_ranked (matches, match_ranks)

        if not best:
            return None

        if len (best) > 1:
            raise NoBestMatchingAlternative ()

        best = best [0]

        original = self.__values[best]

        if value:
            self.__values[best] = value

        return original

#   local rule __test__ ( )
#   {
#       import errors : try catch ;
#       import feature ;
#       import feature : feature subfeature compose ;
#
#       # local rules must be explicitly re-imported
#       import property : path-order ;
#
#       feature.prepare-test property-test-temp ;
#
#       feature toolset : gcc : implicit symmetric ;
#       subfeature toolset gcc : version : 2.95.2 2.95.3 2.95.4
#         3.0 3.0.1 3.0.2 : optional ;
#       feature define : : free ;
#       feature runtime-link : dynamic static : symmetric link-incompatible ;
#       feature optimization : on off ;
#       feature variant : debug release : implicit composite symmetric ;
#       feature rtti : on off : link-incompatible ;
#
#       compose <variant>debug : <define>_DEBUG <optimization>off ;
#       compose <variant>release : <define>NDEBUG <optimization>on ;
#
#       import assert ;
#       import "class" : new ;
#
#       validate <toolset>gcc  <toolset>gcc-3.0.1 : $(test-space) ;
#
#       assert.result <toolset>gcc <rtti>off <define>FOO
#           : refine <toolset>gcc <rtti>off
#           : <define>FOO
#           : $(test-space)
#           ;
#
#       assert.result <toolset>gcc <optimization>on
#           : refine <toolset>gcc <optimization>off
#           : <optimization>on
#           : $(test-space)
#           ;
#
#       assert.result <toolset>gcc <rtti>off
#           : refine <toolset>gcc : <rtti>off : $(test-space)
#           ;
#
#       assert.result <toolset>gcc <rtti>off <rtti>off:<define>FOO
#           : refine <toolset>gcc : <rtti>off <rtti>off:<define>FOO
#           : $(test-space)
#           ;
#
#       assert.result <toolset>gcc:<define>foo <toolset>gcc:<define>bar
#           : refine <toolset>gcc:<define>foo : <toolset>gcc:<define>bar
#           : $(test-space)
#           ;
#
#       assert.result <define>MY_RELEASE
#           : evaluate-conditionals-in-context
#             <variant>release,<rtti>off:<define>MY_RELEASE
#             : <toolset>gcc <variant>release <rtti>off
#
#           ;
#
#       try ;
#           validate <feature>value : $(test-space) ;
#       catch "Invalid property '<feature>value': unknown feature 'feature'." ;
#
#       try ;
#           validate <rtti>default : $(test-space) ;
#       catch \"default\" is not a known value of feature <rtti> ;
#
#       validate <define>WHATEVER : $(test-space) ;
#
#       try ;
#           validate <rtti> : $(test-space) ;
#       catch "Invalid property '<rtti>': No value specified for feature 'rtti'." ;
#
#       try ;
#           validate value : $(test-space) ;
#       catch "value" is not a value of an implicit feature ;
#
#
#       assert.result <rtti>on
#           : remove free implicit : <toolset>gcc <define>foo <rtti>on : $(test-space) ;
#
#       assert.result <include>a
#           : select include : <include>a <toolset>gcc ;
#
#       assert.result <include>a
#           : select include bar : <include>a <toolset>gcc ;
#
#       assert.result <include>a <toolset>gcc
#           : select include <bar> <toolset> : <include>a <toolset>gcc ;
#
#       assert.result <toolset>kylix <include>a
#           : change <toolset>gcc <include>a : <toolset> kylix ;
#
#       # Test ordinary properties
#       assert.result
#         : split-conditional <toolset>gcc
#         ;
#
#       # Test properties with ":"
#       assert.result
#         : split-conditional <define>FOO=A::B
#         ;
#
#       # Test conditional feature
#       assert.result <toolset>gcc,<toolset-gcc:version>3.0 <define>FOO
#         : split-conditional <toolset>gcc,<toolset-gcc:version>3.0:<define>FOO
#         ;
#
#       feature.finish-test property-test-temp ;
#   }
#

