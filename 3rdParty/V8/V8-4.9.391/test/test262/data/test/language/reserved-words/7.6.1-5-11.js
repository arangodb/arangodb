// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-5-11
description: >
    Allow reserved words as property names at object initialization,
    accessed via indexing: enum, extends, super
---*/

        var tokenCodes = {
            enum: 0,
            extends: 1,
            super: 2
        };
        var arr = [
            'enum',
            'extends',
            'super'
        ];  
        for (var i = 0; i < arr.length; i++) {
            assert.sameValue(tokenCodes[arr[i]], i, 'tokenCodes[arr[i]]');
        }
