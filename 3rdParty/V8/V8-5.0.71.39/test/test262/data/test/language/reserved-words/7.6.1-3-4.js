// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-3-4
description: >
    Allow reserved words as property names by index
    assignment,verified with hasOwnProperty: new, var, catch
---*/

        var tokenCodes  = {};
        tokenCodes['new'] = 0;
        tokenCodes['var'] = 1;
        tokenCodes['catch'] = 2;
        var arr = [
            'new',
            'var',
            'catch'
            ];
        for(var p in tokenCodes) {       
            for(var p1 in arr) {                
                if(arr[p1] === p) {
                    assert(tokenCodes.hasOwnProperty(arr[p1]), 'tokenCodes.hasOwnProperty(arr[p1]) !== true');
                }
            }
        }
