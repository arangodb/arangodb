// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-2-16
description: >
    Allow reserved words as property names by dot operator assignment,
    verified with hasOwnProperty: undefined, NaN, Infinity
---*/

        var tokenCodes = {};
        tokenCodes.undefined = 0;
        tokenCodes.NaN = 1;
        tokenCodes.Infinity = 2;
        var arr = [
            'undefined',
            'NaN',
            'Infinity'
        ];
        for(var p in tokenCodes) {       
            for(var p1 in arr) {                
                if(arr[p1] === p) {
                    assert(tokenCodes.hasOwnProperty(arr[p1]), 'tokenCodes.hasOwnProperty(arr[p1]) !== true');
                }
            }
        }
