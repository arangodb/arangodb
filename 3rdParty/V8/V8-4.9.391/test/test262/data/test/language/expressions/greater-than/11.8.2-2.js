// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 11.8.2-2
description: >
    11.8.2 Greater-than Operator - Partial left to right order
    enforced when using Greater-than operator: valueOf > toString
---*/

        var accessed = false;
        var obj1 = {
            valueOf: function () {
                accessed = true;
                return 3;
            }
        };
        var obj2 = {
            toString: function () {
                if (accessed === true) {
                    return 4;
                } else {
                    return 2;
                }
            }
        };

assert(!(obj1 > obj2), '!(obj1 > obj2) !== true');
