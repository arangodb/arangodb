// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-5-16
description: >
    Allow reserved words as property names at object initialization,
    accessed via indexing: undefined, NaN, Infinity
---*/

        var tokenCodes = {
            undefined: 0,
            NaN: 1,
            Infinity: 2
        };
        var arr = [
            'undefined',
            'NaN',
            'Infinity'
        ]; 
        for (var i = 0; i < arr.length; i++) {
            assert.sameValue(tokenCodes[arr[i]], i, 'tokenCodes[arr[i]]');
        }
