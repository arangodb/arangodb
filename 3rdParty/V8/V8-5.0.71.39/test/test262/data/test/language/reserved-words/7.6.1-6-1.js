// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-6-1
description: >
    Allow reserved words as property names by dot operator assignment,
    accessed via indexing: null, true, false
---*/

        var tokenCodes  = {};
        tokenCodes.null = 0;
	    tokenCodes.true = 1;
	    tokenCodes.false = 2; 
        var arr = [
            'null',
            'true',
            'false'
         ];
         for (var i = 0; i < arr.length; i++) {
            assert.sameValue(tokenCodes[arr[i]], i, 'tokenCodes[arr[i]]');
        }
