# Copyright (C) 2018 and later: Unicode, Inc. and others.
# License & terms of use: http://www.unicode.org/copyright.html

from . import common_exec

def run(**kwargs):
    return common_exec.run(is_windows=True, **kwargs)
