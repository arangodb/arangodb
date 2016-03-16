// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-1-11
description: >
    Allow reserved words as property names at object initialization,
    verified with hasOwnProperty: enum, extends, super
---*/

        var tokenCodes  = { 
            enum: 0,
            extends: 1,
            super: 2
        };
        var arr = [
            'enum',
            'extends',
            'super'
        ];        
        for(var p in tokenCodes) {
            for(var p1 in arr) {
                if(arr[p1] === p) {                     
                    assert(tokenCodes.hasOwnProperty(arr[p1]), 'tokenCodes.hasOwnProperty(arr[p1]) !== true');
                }
            }
        }
