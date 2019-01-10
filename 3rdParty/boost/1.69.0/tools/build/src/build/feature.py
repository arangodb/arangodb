# Status: ported, except for unit tests.
# Base revision: 64488
#
# Copyright 2001, 2002, 2003 Dave Abrahams
# Copyright 2002, 2006 Rene Rivera
# Copyright 2002, 2003, 2004, 2005, 2006 Vladimir Prus
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)

import re

from b2.manager import get_manager
from b2.util import utility, bjam_signature, is_iterable_typed
import b2.util.set
from b2.util.utility import add_grist, get_grist, ungrist, replace_grist, to_seq
from b2.exceptions import *

__re_split_subfeatures = re.compile ('<(.*):(.*)>')
__re_no_hyphen = re.compile ('^([^:]+)$')
__re_slash_or_backslash = re.compile (r'[\\/]')

VALID_ATTRIBUTES = {
    'implicit',
    'composite',
    'optional',
    'symmetric',
    'free',
    'incidental',
    'path',
    'dependency',
    'propagated',
    'link-incompatible',
    'subfeature',
    'order-sensitive'
}


class Feature(object):
    def __init__(self, name, values, attributes):
        assert isinstance(name, basestring)
        assert is_iterable_typed(values, basestring)
        assert is_iterable_typed(attributes, basestring)
        self.name = name
        self.values = values
        self.default = None
        self.subfeatures = []
        self.parent = None
        self.attributes_string_list = []
        self._hash = hash(self.name)

        for attr in attributes:
            self.attributes_string_list.append(attr)
            attr = attr.replace("-", "_")
            setattr(self, attr, True)

    def add_values(self, values):
        assert is_iterable_typed(values, basestring)
        self.values.extend(values)

    def set_default(self, value):
        assert isinstance(value, basestring)
        for attr in ('free', 'optional'):
            if getattr(self, attr):
                get_manager().errors()('"{}" feature "<{}>" cannot have a default value.'
                                       .format(attr, self.name))

        self.default = value

    def add_subfeature(self, name):
        assert isinstance(name, Feature)
        self.subfeatures.append(name)

    def set_parent(self, feature, value):
        assert isinstance(feature, Feature)
        assert isinstance(value, basestring)
        self.parent = (feature, value)

    def __hash__(self):
        return self._hash

    def __str__(self):
        return self.name


def reset ():
    """ Clear the module state. This is mainly for testing purposes.
    """
    global __all_attributes, __all_features, __implicit_features, __composite_properties
    global __subfeature_from_value, __all_top_features, __free_features
    global __all_subfeatures

    # sets the default value of False for each valid attribute
    for attr in VALID_ATTRIBUTES:
        setattr(Feature, attr.replace("-", "_"), False)

    # A map containing all features. The key is the feature name.
    # The value is an instance of Feature class.
    __all_features = {}

    # All non-subfeatures.
    __all_top_features = []

    # Maps valus to the corresponding implicit feature
    __implicit_features = {}

    # A map containing all composite properties. The key is a Property instance,
    # and the value is a list of Property instances
    __composite_properties = {}

    # Maps a value to the corresponding subfeature name.
    __subfeature_from_value = {}

    # All free features
    __free_features = []

    __all_subfeatures = []

reset ()

def enumerate ():
    """ Returns an iterator to the features map.
    """
    return __all_features.iteritems ()

def get(name):
    """Return the Feature instance for the specified name.

    Throws if no feature by such name exists
    """
    assert isinstance(name, basestring)
    return __all_features[name]

# FIXME: prepare-test/finish-test?

@bjam_signature((["name"], ["values", "*"], ["attributes", "*"]))
def feature (name, values, attributes = []):
    """ Declares a new feature with the given name, values, and attributes.
        name: the feature name
        values: a sequence of the allowable values - may be extended later with feature.extend
        attributes: a sequence of the feature's attributes (e.g. implicit, free, propagated, ...)
    """
    __validate_feature_attributes (name, attributes)

    feature = Feature(name, [], attributes)
    __all_features[name] = feature
    # Temporary measure while we have not fully moved from 'gristed strings'
    __all_features["<" + name + ">"] = feature

    name = add_grist(name)

    if 'subfeature' in attributes:
        __all_subfeatures.append(name)
    else:
        __all_top_features.append(feature)

    extend (name, values)

    # FIXME: why his is needed.
    if 'free' in attributes:
        __free_features.append (name)

    return feature

@bjam_signature((["feature"], ["value"]))
def set_default (feature, value):
    """ Sets the default value of the given feature, overriding any previous default.
        feature: the name of the feature
        value: the default value to assign
    """
    f = __all_features[feature]
    bad_attribute = None

    if f.free:
        bad_attribute = "free"
    elif f.optional:
        bad_attribute = "optional"

    if bad_attribute:
        raise InvalidValue ("%s property %s cannot have a default" % (bad_attribute, f.name))

    if value not in f.values:
        raise InvalidValue ("The specified default value, '%s' is invalid.\n" % value + "allowed values are: %s" % f.values)

    f.set_default(value)

def defaults(features):
    """ Returns the default property values for the given features.
    """
    assert is_iterable_typed(features, Feature)
    # FIXME: should merge feature and property modules.
    from . import property

    result = []
    for f in features:
        if not f.free and not f.optional and f.default:
            result.append(property.Property(f, f.default))

    return result

def valid (names):
    """ Returns true iff all elements of names are valid features.
    """
    if isinstance(names, str):
        names = [names]
        assert is_iterable_typed(names, basestring)

    return all(name in __all_features for name in names)

def attributes (feature):
    """ Returns the attributes of the given feature.
    """
    assert isinstance(feature, basestring)
    return __all_features[feature].attributes_string_list

def values (feature):
    """ Return the values of the given feature.
    """
    assert isinstance(feature, basestring)
    validate_feature (feature)
    return __all_features[feature].values

def is_implicit_value (value_string):
    """ Returns true iff 'value_string' is a value_string
    of an implicit feature.
    """
    assert isinstance(value_string, basestring)
    if value_string in __implicit_features:
        return __implicit_features[value_string]

    v = value_string.split('-')

    if v[0] not in __implicit_features:
        return False

    feature = __implicit_features[v[0]]

    for subvalue in (v[1:]):
        if not __find_implied_subfeature(feature, subvalue, v[0]):
            return False

    return True

def implied_feature (implicit_value):
    """ Returns the implicit feature associated with the given implicit value.
    """
    assert isinstance(implicit_value, basestring)
    components = implicit_value.split('-')

    if components[0] not in __implicit_features:
        raise InvalidValue ("'%s' is not a value of an implicit feature" % implicit_value)

    return __implicit_features[components[0]]

def __find_implied_subfeature (feature, subvalue, value_string):
    assert isinstance(feature, Feature)
    assert isinstance(subvalue, basestring)
    assert isinstance(value_string, basestring)

    try:
        return __subfeature_from_value[feature][value_string][subvalue]
    except KeyError:
        return None

# Given a feature and a value of one of its subfeatures, find the name
# of the subfeature. If value-string is supplied, looks for implied
# subfeatures that are specific to that value of feature
#  feature             # The main feature name
#  subvalue            # The value of one of its subfeatures
#  value-string        # The value of the main feature

def implied_subfeature (feature, subvalue, value_string):
    assert isinstance(feature, Feature)
    assert isinstance(subvalue, basestring)
    assert isinstance(value_string, basestring)
    result = __find_implied_subfeature (feature, subvalue, value_string)
    if not result:
        raise InvalidValue ("'%s' is not a known subfeature value of '%s%s'" % (subvalue, feature, value_string))

    return result

def validate_feature (name):
    """ Checks if all name is a valid feature. Otherwise, raises an exception.
    """
    assert isinstance(name, basestring)
    if name not in __all_features:
        raise InvalidFeature ("'%s' is not a valid feature name" % name)
    else:
        return __all_features[name]


# Uses Property
def __expand_subfeatures_aux (property_, dont_validate = False):
    """ Helper for expand_subfeatures.
        Given a feature and value, or just a value corresponding to an
        implicit feature, returns a property set consisting of all component
        subfeatures and their values. For example:

          expand_subfeatures <toolset>gcc-2.95.2-linux-x86
              -> <toolset>gcc <toolset-version>2.95.2 <toolset-os>linux <toolset-cpu>x86
          equivalent to:
              expand_subfeatures gcc-2.95.2-linux-x86

        feature:        The name of the feature, or empty if value corresponds to an implicit property
        value:          The value of the feature.
        dont_validate:  If True, no validation of value string will be done.
    """
    from . import property  # no __debug__ since Property is used elsewhere
    assert isinstance(property_, property.Property)
    assert isinstance(dont_validate, int)  # matches bools

    f = property_.feature
    v = property_.value
    if not dont_validate:
        validate_value_string(f, v)

    components = v.split ("-")

    v = components[0]

    result = [property.Property(f, components[0])]

    subvalues = components[1:]

    while len(subvalues) > 0:
        subvalue = subvalues [0]    # pop the head off of subvalues
        subvalues = subvalues [1:]

        subfeature = __find_implied_subfeature (f, subvalue, v)

        # If no subfeature was found, reconstitute the value string and use that
        if not subfeature:
            return [property.Property(f, '-'.join(components))]

        result.append(property.Property(subfeature, subvalue))

    return result

def expand_subfeatures(properties, dont_validate = False):
    """
    Make all elements of properties corresponding to implicit features
    explicit, and express all subfeature values as separate properties
    in their own right. For example, the property

       gcc-2.95.2-linux-x86

    might expand to

      <toolset>gcc <toolset-version>2.95.2 <toolset-os>linux <toolset-cpu>x86

    properties:     A sequence with elements of the form
                    <feature>value-string or just value-string in the
                    case of implicit features.
  : dont_validate:  If True, no validation of value string will be done.
    """
    if __debug__:
        from .property import Property
        assert is_iterable_typed(properties, Property)
        assert isinstance(dont_validate, int)  # matches bools
    result = []
    for p in properties:
        # Don't expand subfeatures in subfeatures
        if p.feature.subfeature:
            result.append (p)
        else:
            result.extend(__expand_subfeatures_aux (p, dont_validate))

    return result



# rule extend was defined as below:
    # Can be called three ways:
    #
    #    1. extend feature : values *
    #    2. extend <feature> subfeature : values *
    #    3. extend <feature>value-string subfeature : values *
    #
    # * Form 1 adds the given values to the given feature
    # * Forms 2 and 3 add subfeature values to the given feature
    # * Form 3 adds the subfeature values as specific to the given
    #   property value-string.
    #
    #rule extend ( feature-or-property subfeature ? : values * )
#
# Now, the specific rule must be called, depending on the desired operation:
#   extend_feature
#   extend_subfeature
@bjam_signature([['name'], ['values', '*']])
def extend (name, values):
    """ Adds the given values to the given feature.
    """
    assert isinstance(name, basestring)
    assert is_iterable_typed(values, basestring)
    name = add_grist (name)
    __validate_feature (name)
    feature = __all_features [name]

    if feature.implicit:
        for v in values:
            if v in __implicit_features:
                raise BaseException ("'%s' is already associated with the feature '%s'" % (v, __implicit_features [v]))

            __implicit_features[v] = feature

    if values and not feature.values and not(feature.free or feature.optional):
        # This is the first value specified for this feature,
        # take it as default value
        feature.set_default(values[0])

    feature.add_values(values)

def validate_value_string (f, value_string):
    """ Checks that value-string is a valid value-string for the given feature.
    """
    assert isinstance(f, Feature)
    assert isinstance(value_string, basestring)
    if f.free or value_string in f.values:
        return

    values = [value_string]

    if f.subfeatures:
        if not value_string in f.values and \
               not value_string in f.subfeatures:
            values = value_string.split('-')

    # An empty value is allowed for optional features
    if not values[0] in f.values and \
           (values[0] or not f.optional):
        raise InvalidValue ("'%s' is not a known value of feature '%s'\nlegal values: '%s'" % (values [0], f.name, f.values))

    for v in values [1:]:
        # this will validate any subfeature values in value-string
        implied_subfeature(f, v, values[0])


""" Extends the given subfeature with the subvalues.  If the optional
    value-string is provided, the subvalues are only valid for the given
    value of the feature. Thus, you could say that
    <target-platform>mingw is specific to <toolset>gcc-2.95.2 as follows:

          extend-subfeature toolset gcc-2.95.2 : target-platform : mingw ;

    feature:        The feature whose subfeature is being extended.

    value-string:   If supplied, specifies a specific value of the
                    main feature for which the new subfeature values
                    are valid.

    subfeature:     The name of the subfeature.

    subvalues:      The additional values of the subfeature being defined.
"""
def extend_subfeature (feature_name, value_string, subfeature_name, subvalues):
    assert isinstance(feature_name, basestring)
    assert isinstance(value_string, basestring)
    assert isinstance(subfeature_name, basestring)
    assert is_iterable_typed(subvalues, basestring)
    feature = validate_feature(feature_name)

    if value_string:
        validate_value_string(feature, value_string)

    subfeature_name = feature_name + '-' + __get_subfeature_name (subfeature_name, value_string)

    extend(subfeature_name, subvalues) ;
    subfeature = __all_features[subfeature_name]

    if value_string == None: value_string = ''

    if feature not in __subfeature_from_value:
        __subfeature_from_value[feature] = {}

    if value_string not in __subfeature_from_value[feature]:
        __subfeature_from_value[feature][value_string] = {}

    for subvalue in subvalues:
        __subfeature_from_value [feature][value_string][subvalue] = subfeature

@bjam_signature((["feature_name", "value_string", "?"], ["subfeature"],
                 ["subvalues", "*"], ["attributes", "*"]))
def subfeature (feature_name, value_string, subfeature, subvalues, attributes = []):
    """ Declares a subfeature.
        feature_name:   Root feature that is not a subfeature.
        value_string:   An optional value-string specifying which feature or
                        subfeature values this subfeature is specific to,
                        if any.
        subfeature:     The name of the subfeature being declared.
        subvalues:      The allowed values of this subfeature.
        attributes:     The attributes of the subfeature.
    """
    parent_feature = validate_feature (feature_name)

    # Add grist to the subfeature name if a value-string was supplied
    subfeature_name = __get_subfeature_name (subfeature, value_string)

    if subfeature_name in __all_features[feature_name].subfeatures:
        message = "'%s' already declared as a subfeature of '%s'" % (subfeature, feature_name)
        message += " specific to '%s'" % value_string
        raise BaseException (message)

    # First declare the subfeature as a feature in its own right
    f = feature (feature_name + '-' + subfeature_name, subvalues, attributes + ['subfeature'])
    f.set_parent(parent_feature, value_string)

    parent_feature.add_subfeature(f)

    # Now make sure the subfeature values are known.
    extend_subfeature (feature_name, value_string, subfeature, subvalues)


@bjam_signature((["composite_property_s"], ["component_properties_s", "*"]))
def compose (composite_property_s, component_properties_s):
    """ Sets the components of the given composite property.

    All parameters are <feature>value strings
    """
    from . import property

    component_properties_s = to_seq (component_properties_s)
    composite_property = property.create_from_string(composite_property_s)
    f = composite_property.feature

    if len(component_properties_s) > 0 and isinstance(component_properties_s[0], property.Property):
        component_properties = component_properties_s
    else:
        component_properties = [property.create_from_string(p) for p in component_properties_s]

    if not f.composite:
        raise BaseException ("'%s' is not a composite feature" % f)

    if property in __composite_properties:
        raise BaseException ('components of "%s" already set: %s' % (composite_property, str (__composite_properties[composite_property])))

    if composite_property in component_properties:
        raise BaseException ('composite property "%s" cannot have itself as a component' % composite_property)

    __composite_properties[composite_property] = component_properties


def expand_composite(property_):
    if __debug__:
        from .property import Property
        assert isinstance(property_, Property)
    result = [ property_ ]
    if property_ in __composite_properties:
        for p in __composite_properties[property_]:
            result.extend(expand_composite(p))
    return result

@bjam_signature((['feature'], ['properties', '*']))
def get_values (feature, properties):
    """ Returns all values of the given feature specified by the given property set.
    """
    if feature[0] != '<':
     feature = '<' + feature + '>'
    result = []
    for p in properties:
        if get_grist (p) == feature:
            result.append (replace_grist (p, ''))

    return result

def free_features ():
    """ Returns all free features.
    """
    return __free_features

def expand_composites (properties):
    """ Expand all composite properties in the set so that all components
        are explicitly expressed.
    """
    if __debug__:
        from .property import Property
        assert is_iterable_typed(properties, Property)
    explicit_features = set(p.feature for p in properties)

    result = []

    # now expand composite features
    for p in properties:
        expanded = expand_composite(p)

        for x in expanded:
            if not x in result:
                f = x.feature

                if f.free:
                    result.append (x)
                elif not x in properties:  # x is the result of expansion
                    if not f in explicit_features:  # not explicitly-specified
                        if any(r.feature == f for r in result):
                            raise FeatureConflict(
                                "expansions of composite features result in "
                                "conflicting values for '%s'\nvalues: '%s'\none contributing composite property was '%s'" %
                                (f.name, [r.value for r in result if r.feature == f] + [x.value], p))
                        else:
                            result.append (x)
                elif any(r.feature == f for r in result):
                    raise FeatureConflict ("explicitly-specified values of non-free feature '%s' conflict\n"
                    "existing values: '%s'\nvalue from expanding '%s': '%s'" % (f,
                    [r.value for r in result if r.feature == f], p, x.value))
                else:
                    result.append (x)

    return result

# Uses Property
def is_subfeature_of (parent_property, f):
    """ Return true iff f is an ordinary subfeature of the parent_property's
        feature, or if f is a subfeature of the parent_property's feature
        specific to the parent_property's value.
    """
    if __debug__:
        from .property import Property
        assert isinstance(parent_property, Property)
        assert isinstance(f, Feature)

    if not f.subfeature:
        return False

    p = f.parent
    if not p:
        return False

    parent_feature = p[0]
    parent_value = p[1]

    if parent_feature != parent_property.feature:
        return False

    if parent_value and parent_value != parent_property.value:
        return False

    return True

def __is_subproperty_of (parent_property, p):
    """ As is_subfeature_of, for subproperties.
    """
    if __debug__:
        from .property import Property
        assert isinstance(parent_property, Property)
        assert isinstance(p, Property)
    return is_subfeature_of (parent_property, p.feature)


# Returns true iff the subvalue is valid for the feature.  When the
# optional value-string is provided, returns true iff the subvalues
# are valid for the given value of the feature.
def is_subvalue(feature, value_string, subfeature, subvalue):
    assert isinstance(feature, basestring)
    assert isinstance(value_string, basestring)
    assert isinstance(subfeature, basestring)
    assert isinstance(subvalue, basestring)
    if not value_string:
        value_string = ''
    try:
        return  __subfeature_from_value[feature][value_string][subvalue] == subfeature
    except KeyError:
        return False


# Uses Property
def expand (properties):
    """ Given a property set which may consist of composite and implicit
        properties and combined subfeature values, returns an expanded,
        normalized property set with all implicit features expressed
        explicitly, all subfeature values individually expressed, and all
        components of composite properties expanded. Non-free features
        directly expressed in the input properties cause any values of
        those features due to composite feature expansion to be dropped. If
        two values of a given non-free feature are directly expressed in the
        input, an error is issued.
    """
    if __debug__:
        from .property import Property
        assert is_iterable_typed(properties, Property)
    expanded = expand_subfeatures(properties)
    return expand_composites (expanded)

# Accepts list of Property objects
def add_defaults (properties):
    """ Given a set of properties, add default values for features not
        represented in the set.
        Note: if there's there's ordinary feature F1 and composite feature
        F2, which includes some value for F1, and both feature have default values,
        then the default value of F1 will be added, not the value in F2. This might
        not be right idea: consider

          feature variant : debug ... ;
               <variant>debug : .... <runtime-debugging>on
          feature <runtime-debugging> : off on ;

          Here, when adding default for an empty property set, we'll get

            <variant>debug <runtime_debugging>off

          and that's kind of strange.
    """
    if __debug__:
        from .property import Property
        assert is_iterable_typed(properties, Property)
    # create a copy since properties will be modified
    result = list(properties)

    # We don't add default for conditional properties.  We don't want
    # <variant>debug:<define>DEBUG to be takes as specified value for <variant>
    handled_features = set(p.feature for p in properties if not p.condition)

    missing_top = [f for f in __all_top_features if not f in handled_features]
    more = defaults(missing_top)
    result.extend(more)
    handled_features.update(p.feature for p in more)

    # Add defaults for subfeatures of features which are present
    for p in result[:]:
        subfeatures = [s for s in p.feature.subfeatures if not s in handled_features]
        more = defaults(__select_subfeatures(p, subfeatures))
        handled_features.update(h.feature for h in more)
        result.extend(more)

    return result

def minimize (properties):
    """ Given an expanded property set, eliminate all redundancy: properties
        which are elements of other (composite) properties in the set will
        be eliminated. Non-symmetric properties equal to default values will be
        eliminated, unless the override a value from some composite property.
        Implicit properties will be expressed without feature
        grist, and sub-property values will be expressed as elements joined
        to the corresponding main property.
    """
    if __debug__:
        from .property import Property
        assert is_iterable_typed(properties, Property)
    # remove properties implied by composite features
    components = []
    component_features = set()
    for property in properties:
        if property in __composite_properties:
            cs = __composite_properties[property]
            components.extend(cs)
            component_features.update(c.feature for c in cs)

    properties = b2.util.set.difference (properties, components)

    # handle subfeatures and implicit features

    # move subfeatures to the end of the list
    properties = [p for p in properties if not p.feature.subfeature] +\
        [p for p in properties if p.feature.subfeature]

    result = []
    while properties:
        p = properties[0]
        f = p.feature

        # locate all subproperties of $(x[1]) in the property set
        subproperties = [x for x in properties if is_subfeature_of(p, x.feature)]

        if subproperties:
            # reconstitute the joined property name
            subproperties.sort ()
            joined = b2.build.property.Property(p.feature, p.value + '-' + '-'.join ([sp.value for sp in subproperties]))
            result.append(joined)

            properties = b2.util.set.difference(properties[1:], subproperties)

        else:
            # eliminate properties whose value is equal to feature's
            # default and which are not symmetric and which do not
            # contradict values implied by composite properties.

            # since all component properties of composites in the set
            # have been eliminated, any remaining property whose
            # feature is the same as a component of a composite in the
            # set must have a non-redundant value.
            if p.value != f.default or f.symmetric or f in component_features:
                result.append (p)

            properties = properties[1:]

    return result


def split (properties):
    """ Given a property-set of the form
        v1/v2/...vN-1/<fN>vN/<fN+1>vN+1/...<fM>vM

    Returns
        v1 v2 ... vN-1 <fN>vN <fN+1>vN+1 ... <fM>vM

    Note that vN...vM may contain slashes. This is resilient to the
    substitution of backslashes for slashes, since Jam, unbidden,
    sometimes swaps slash direction on NT.
    """
    assert isinstance(properties, basestring)
    def split_one (properties):
        pieces = re.split (__re_slash_or_backslash, properties)
        result = []

        for x in pieces:
            if not get_grist (x) and len (result) > 0 and get_grist (result [-1]):
                result = result [0:-1] + [ result [-1] + '/' + x ]
            else:
                result.append (x)

        return result

    if isinstance (properties, str):
        return split_one (properties)

    result = []
    for p in properties:
        result += split_one (p)
    return result


def compress_subproperties (properties):
    """ Combine all subproperties into their parent properties

        Requires: for every subproperty, there is a parent property.  All
        features are explicitly expressed.

        This rule probably shouldn't be needed, but
        build-request.expand-no-defaults is being abused for unintended
        purposes and it needs help
    """
    from .property import Property
    assert is_iterable_typed(properties, Property)
    result = []
    matched_subs = set()
    all_subs = set()
    for p in properties:
        f = p.feature

        if not f.subfeature:
            subs = [x for x in properties if is_subfeature_of(p, x.feature)]
            if subs:

                matched_subs.update(subs)

                subvalues = '-'.join (sub.value for sub in subs)
                result.append(Property(
                    p.feature, p.value + '-' + subvalues,
                    p.condition))
            else:
                result.append(p)

        else:
            all_subs.add(p)

    # TODO: this variables are used just for debugging. What's the overhead?
    assert all_subs == matched_subs

    return result

######################################################################################
# Private methods

def __select_subproperties (parent_property, properties):
    if __debug__:
        from .property import Property
        assert is_iterable_typed(properties, Property)
        assert isinstance(parent_property, Property)
    return [ x for x in properties if __is_subproperty_of (parent_property, x) ]

def __get_subfeature_name (subfeature, value_string):
    assert isinstance(subfeature, basestring)
    assert isinstance(value_string, basestring) or value_string is None
    if value_string == None:
        prefix = ''
    else:
        prefix = value_string + ':'

    return prefix + subfeature


def __validate_feature_attributes (name, attributes):
    assert isinstance(name, basestring)
    assert is_iterable_typed(attributes, basestring)
    for attribute in attributes:
        if attribute not in VALID_ATTRIBUTES:
            raise InvalidAttribute ("unknown attributes: '%s' in feature declaration: '%s'" % (str (b2.util.set.difference (attributes, __all_attributes)), name))

    if name in __all_features:
            raise AlreadyDefined ("feature '%s' already defined" % name)
    elif 'implicit' in attributes and 'free' in attributes:
        raise InvalidAttribute ("free features cannot also be implicit (in declaration of feature '%s')" % name)
    elif 'free' in attributes and 'propagated' in attributes:
        raise InvalidAttribute ("free features cannot also be propagated (in declaration of feature '%s')" % name)


def __validate_feature (feature):
    """ Generates an error if the feature is unknown.
    """
    assert isinstance(feature, basestring)
    if feature not in __all_features:
        raise BaseException ('unknown feature "%s"' % feature)


def __select_subfeatures (parent_property, features):
    """ Given a property, return the subset of features consisting of all
        ordinary subfeatures of the property's feature, and all specific
        subfeatures of the property's feature which are conditional on the
        property's value.
    """
    if __debug__:
        from .property import Property
        assert isinstance(parent_property, Property)
        assert is_iterable_typed(features, Feature)
    return [f for f in features if is_subfeature_of (parent_property, f)]

# FIXME: copy over tests.
