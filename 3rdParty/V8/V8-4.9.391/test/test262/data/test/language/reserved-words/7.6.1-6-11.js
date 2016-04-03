// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-6-11
description: >
    Allow reserved words as property names by dot operator assignment,
    accessed via indexing: enum, extends, super
---*/

        var tokenCodes  = {};
        tokenCodes.enum = 0;
        tokenCodes.extends = 1;
        tokenCodes.super = 2;
        var arr = [
            'enum',
            'extends',
            'super'
         ];
         for (var i = 0; i < arr.length; i++) {
            assert.sameValue(tokenCodes[arr[i]], i, 'tokenCodes[arr[i]]');
        }
