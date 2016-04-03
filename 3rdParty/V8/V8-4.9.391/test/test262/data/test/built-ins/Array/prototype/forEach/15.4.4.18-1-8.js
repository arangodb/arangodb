// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.18-1-8
description: Array.prototype.forEach applied to String object
---*/

        var result = false;
        function callbackfn(val, idx, obj) {
            result = obj instanceof String;
        }

        var obj = new String("abc");
        Array.prototype.forEach.call(obj, callbackfn);

assert(result, 'result !== true');
