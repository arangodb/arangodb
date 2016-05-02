// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-5-12
description: >
    Allow reserved words as property names at object initialization,
    accessed via indexing: const, export, import
---*/

        var tokenCodes = {
            const : 0,
            export: 1,
            import: 2
        };
        var arr = [
            'const',
            'export',
            'import'
        ]; 
        for (var i = 0; i < arr.length; i++) {
            assert.sameValue(tokenCodes[arr[i]], i, 'tokenCodes[arr[i]]');
        }
