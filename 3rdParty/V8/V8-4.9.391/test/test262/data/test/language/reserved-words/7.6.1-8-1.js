// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-8-1
description: >
    Allow reserved words as property names by set function within an
    object, accessed via indexing: null, true, false
---*/

        var test0 = 0, test1 = 1, test2 = 2;
        var tokenCodes  = {
            set null(value) {
                test0 = value;        
            },
            get null() {
                return test0;
            },
            set true(value) {
                test1 = value;        
            },
            get true() {
                return test1;
            },
            set false(value) {
                test2 = value;        
            },
            get false(){
                return test2;
            }
        }; 
        var arr = [
            'null',
            'true',
            'false'
        ];
        for (var i = 0; i < arr.length; i++) {
            assert.sameValue(tokenCodes[arr[i]], i, 'tokenCodes[arr[i]]');
        }
