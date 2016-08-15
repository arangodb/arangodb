
import bjam
import re
import types

from itertools import groupby


def safe_isinstance(value, types=None, class_names=None):
    """To prevent circular imports, this extends isinstance()
    by checking also if `value` has a particular class name (or inherits from a
    particular class name). This check is safe in that an AttributeError is not
    raised in case `value` doesn't have a __class__ attribute.
    """
    # inspect is being imported here because I seriously doubt
    # that this function will be used outside of the type
    # checking below.
    import inspect
    result = False
    if types is not None:
        result = result or isinstance(value, types)
    if class_names is not None and not result:
        # this doesn't work with inheritance, but normally
        # either the class will already be imported within the module,
        # or the class doesn't have any subclasses. For example: PropertySet
        if isinstance(class_names, basestring):
            class_names = [class_names]
        # this is the part that makes it "safe".
        try:
            base_names = [class_.__name__ for class_ in inspect.getmro(value.__class__)]
            for name in class_names:
                if name in base_names:
                    return True
        except AttributeError:
            pass
    return result


def is_iterable_typed(values, type_):
    return is_iterable(values) and all(isinstance(v, type_) for v in values)


def is_iterable(value):
    """Returns whether value is iterable and not a string."""
    return not isinstance(value, basestring) and hasattr(value, '__iter__')


def is_iterable_or_none(value):
    return is_iterable(value) or value is None


def is_single_value(value):
    # some functions may specify a bjam signature
    # that is a string type, but still allow a
    # PropertySet to be passed in
    return safe_isinstance(value, (basestring, type(None)), 'PropertySet')


if __debug__:

    from textwrap import dedent
    message = dedent(
        """The parameter "{}" was passed in a wrong type for the "{}()" function.
        Actual:
        \ttype: {}
        \tvalue: {}
        Expected:
        \t{}
        """
    )

    bjam_types = {
        '*': is_iterable_or_none,
        '+': is_iterable_or_none,
        '?': is_single_value,
        '': is_single_value,
    }

    bjam_to_python = {
        '*': 'iterable',
        '+': 'iterable',
        '?': 'single value',
        '': 'single value',
    }


    def get_next_var(field):
        it = iter(field)
        var = it.next()
        type_ = None
        yield_var = False
        while type_ not in bjam_types:
            try:
                # the first value has already
                # been consumed outside of the loop
                type_ = it.next()
            except StopIteration:
                # if there are no more values, then
                # var still needs to be returned
                yield_var = True
                break
            if type_ not in bjam_types:
                # type_ is not a type and is
                # another variable in the same field.
                yield var, ''
                # type_ is the next var
                var = type_
            else:
                # otherwise, type_ is a type for var
                yield var, type_
                try:
                    # the next value should be a var
                    var = it.next()
                except StopIteration:
                    # if not, then we're done with
                    # this field
                    break
        if yield_var:
            yield var, ''


# Decorator the specifies bjam-side prototype for a Python function
def bjam_signature(s):
    if __debug__:
        from inspect import getcallargs
        def decorator(fn):
            function_name = fn.__module__ + '.' + fn.__name__
            def wrapper(*args, **kwargs):
                callargs = getcallargs(fn, *args, **kwargs)
                for field in s:
                    for var, type_ in get_next_var(field):
                        try:
                            value = callargs[var]
                        except KeyError:
                            raise Exception(
                                'Bjam Signature specifies a variable named "{}"\n'
                                'but is not found within the python function signature\n'
                                'for function {}()'.format(var, function_name)
                            )
                        if not bjam_types[type_](value):
                            raise TypeError(
                                message.format(var, function_name, type(type_), repr(value),
                                               bjam_to_python[type_])
                            )
                return fn(*args, **kwargs)
            wrapper.__name__ = fn.__name__
            wrapper.bjam_signature = s
            return wrapper
        return decorator
    else:
        def decorator(f):
            f.bjam_signature = s
            return f

    return decorator

def metatarget(f):

    f.bjam_signature = (["name"], ["sources", "*"], ["requirements", "*"],
                        ["default_build", "*"], ["usage_requirements", "*"])
    return f

class cached(object):

    def __init__(self, function):
        self.function = function
        self.cache = {}

    def __call__(self, *args):
        try:
            return self.cache[args]
        except KeyError:
            v = self.function(*args)
            self.cache[args] = v
            return v

    def __get__(self, instance, type):
        return types.MethodType(self, instance, type)

def unquote(s):
    if s and s[0] == '"' and s[-1] == '"':
        return s[1:-1]
    else:
        return s

_extract_jamfile_and_rule = re.compile("(Jamfile<.*>)%(.*)")

def qualify_jam_action(action_name, context_module):

    if action_name.startswith("###"):
        # Callable exported from Python. Don't touch
        return action_name
    elif _extract_jamfile_and_rule.match(action_name):
        # Rule is already in indirect format
        return action_name
    else:
        ix = action_name.find('.')
        if ix != -1 and action_name[:ix] == context_module:
            return context_module + '%' + action_name[ix+1:]

        return context_module + '%' + action_name


def set_jam_action(name, *args):

    m = _extract_jamfile_and_rule.match(name)
    if m:
        args = ("set-update-action-in-module", m.group(1), m.group(2)) + args
    else:
        args = ("set-update-action", name) + args

    return bjam.call(*args)


def call_jam_function(name, *args):

    m = _extract_jamfile_and_rule.match(name)
    if m:
        args = ("call-in-module", m.group(1), m.group(2)) + args
        return bjam.call(*args)
    else:
        return bjam.call(*((name,) + args))

__value_id = 0
__python_to_jam = {}
__jam_to_python = {}

def value_to_jam(value, methods=False):
    """Makes a token to refer to a Python value inside Jam language code.

    The token is merely a string that can be passed around in Jam code and
    eventually passed back. For example, we might want to pass PropertySet
    instance to a tag function and it might eventually call back
    to virtual_target.add_suffix_and_prefix, passing the same instance.

    For values that are classes, we'll also make class methods callable
    from Jam.

    Note that this is necessary to make a bit more of existing Jamfiles work.
    This trick should not be used to much, or else the performance benefits of
    Python port will be eaten.
    """

    global __value_id

    r = __python_to_jam.get(value, None)
    if r:
        return r

    exported_name = '###_' + str(__value_id)
    __value_id = __value_id + 1
    __python_to_jam[value] = exported_name
    __jam_to_python[exported_name] = value

    if methods and type(value) == types.InstanceType:
        for field_name in dir(value):
            field = getattr(value, field_name)
            if callable(field) and not field_name.startswith("__"):
                bjam.import_rule("", exported_name + "." + field_name, field)

    return exported_name

def record_jam_to_value_mapping(jam_value, python_value):
    __jam_to_python[jam_value] = python_value

def jam_to_value_maybe(jam_value):

    if type(jam_value) == type(""):
        return __jam_to_python.get(jam_value, jam_value)
    else:
        return jam_value

def stem(filename):
    i = filename.find('.')
    if i != -1:
        return filename[0:i]
    else:
        return filename


def abbreviate_dashed(s):
    """Abbreviates each part of string that is delimited by a '-'."""
    r = []
    for part in s.split('-'):
        r.append(abbreviate(part))
    return '-'.join(r)


def abbreviate(s):
    """Apply a set of standard transformations to string to produce an
    abbreviation no more than 4 characters long.
    """
    if not s:
        return ''
    # check the cache
    if s in abbreviate.abbreviations:
        return abbreviate.abbreviations[s]
    # anything less than 4 characters doesn't need
    # an abbreviation
    if len(s) < 4:
        # update cache
        abbreviate.abbreviations[s] = s
        return s
    # save the first character in case it's a vowel
    s1 = s[0]
    s2 = s[1:]
    if s.endswith('ing'):
        # strip off the 'ing'
        s2 = s2[:-3]
    # reduce all doubled characters to one
    s2 = ''.join(c for c, _ in groupby(s2))
    # remove all vowels
    s2 = s2.translate(None, "AEIOUaeiou")
    # shorten remaining consonants to 4 characters
    # and add the first char back to the front
    s2 = s1 + s2[:4]
    # update cache
    abbreviate.abbreviations[s] = s2
    return s2
# maps key to its abbreviated form
abbreviate.abbreviations = {}
