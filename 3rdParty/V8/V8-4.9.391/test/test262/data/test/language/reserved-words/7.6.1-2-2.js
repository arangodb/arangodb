// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-2-2
description: >
    Allow reserved words as property names by dot operator assignment,
    verified with hasOwnProperty: break, case, do
---*/

        var tokenCodes  = {};
        tokenCodes.break = 0;  	
        tokenCodes.case = 1;
        tokenCodes.do = 2;
        var arr = [
            'break',
            'case',
            'do'
        ];
        for(var p in tokenCodes) {       
            for(var p1 in arr) {                
                if(arr[p1] === p) {
                    assert(tokenCodes.hasOwnProperty(arr[p1]), 'tokenCodes.hasOwnProperty(arr[p1]) !== true');
                }
            }
        }
