# Copyright Pedro Ferreira 2005. Distributed under the Boost
# Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


class BaseBoostBuildException(Exception):
    """A base Exception class for all other Boost.Build exceptions to inherit from."""


class UserError(BaseBoostBuildException):
    pass


class FeatureConflict(BaseBoostBuildException):
    pass


class InvalidSource(BaseBoostBuildException):
    pass


class InvalidFeature(BaseBoostBuildException):
    pass


class InvalidProperty(BaseBoostBuildException):
    pass


class InvalidValue(BaseBoostBuildException):
    pass


class InvalidAttribute(BaseBoostBuildException):
    pass


class AlreadyDefined(BaseBoostBuildException):
    pass


class IllegalOperation(BaseBoostBuildException):
    pass


class Recursion(BaseBoostBuildException):
    pass


class NoBestMatchingAlternative(BaseBoostBuildException):
    pass


class NoAction(BaseBoostBuildException):
    pass
