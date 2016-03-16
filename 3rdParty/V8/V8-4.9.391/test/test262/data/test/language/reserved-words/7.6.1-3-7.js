// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-3-7
description: >
    Allow reserved words as property names by index
    assignment,verified with hasOwnProperty: while, debugger, function
---*/

        var tokenCodes  = {};
        tokenCodes['while'] = 0;
        tokenCodes['debugger'] = 1;
        tokenCodes['function'] = 2; 
        var arr = [
            'while',
            'debugger',
            'function'
            ];
        for(var p in tokenCodes) {       
            for(var p1 in arr) {                
                if(arr[p1] === p) {
                    assert(tokenCodes.hasOwnProperty(arr[p1]), 'tokenCodes.hasOwnProperty(arr[p1]) !== true');
                }
            }
        }
