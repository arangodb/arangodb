// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-1-15
description: >
    Allow reserved words as property names at object initialization,
    verified with hasOwnProperty: package, protected, static
---*/

        var tokenCodes  = { 
            package: 0,
            protected: 1,
            static: 2
        };
        var arr = [
            'package',
            'protected',
            'static'
        ];        
        for(var p in tokenCodes) {
            for(var p1 in arr) {
                if(arr[p1] === p) {                     
                    assert(tokenCodes.hasOwnProperty(arr[p1]), 'tokenCodes.hasOwnProperty(arr[p1]) !== true');
                }
            }
        }
