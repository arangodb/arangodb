// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.15-8-b-i-25
description: >
    Array.prototype.lastIndexOf applied to Arguments object which
    implements its own property get method (number of arguments is
    less than number of parameters)
---*/

        var func = function (a, b) {
            return 0 === Array.prototype.lastIndexOf.call(arguments, arguments[0]) &&
                -1 === Array.prototype.lastIndexOf.call(arguments, arguments[1]);
        };

assert(func(true), 'func(true) !== true');
