// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-7-2
description: >
    Allow reserved words as property names by index assignment,
    accessed via indexing: break, case, do
---*/

        var tokenCodes = {};
        tokenCodes['break'] = 0;
        tokenCodes['case'] = 1;
        tokenCodes['do'] = 2;     
        var arr = [
            'break',
            'case',
            'do'
        ];
        for (var i = 0; i < arr.length; i++) {
            assert.sameValue(tokenCodes[arr[i]], i, 'tokenCodes[arr[i]]');
        }
