// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-3-6
description: >
    Allow reserved words as property names by index
    assignment,verified with hasOwnProperty: continue, for, switch
---*/

        var tokenCodes  = {};
        tokenCodes['continue'] = 0;
        tokenCodes['for'] = 1;
        tokenCodes['switch'] = 2;
        var arr = [
            'continue',
            'for',
            'switch'
            ];
        for(var p in tokenCodes) {       
            for(var p1 in arr) {                
                if(arr[p1] === p) {
                    assert(tokenCodes.hasOwnProperty(arr[p1]), 'tokenCodes.hasOwnProperty(arr[p1]) !== true');
                }
            }
        }
