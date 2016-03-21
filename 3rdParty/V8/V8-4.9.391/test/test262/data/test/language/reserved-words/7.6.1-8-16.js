// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-8-16
description: >
    Allow reserved words as property names by set function within an
    object, accessed via indexing: undefined, NaN, Infinity
---*/

        var test0 = 0, test1 = 1, test2 = 2;
        var tokenCodes  = {
            set undefined(value){
                test0 = value;
            },
            get undefined(){
                return test0;
            },
            set NaN(value){
                test1 = value;
            },
            get NaN(){
                return test1;
            },
            set Infinity(value){
                test2 = value;
            },
            get Infinity(){
                return test2;
            }
        }; 
        var arr = [
            'undefined',
            'NaN',
            'Infinity'
        ];
        for (var i = 0; i < arr.length; i++) {
            assert.sameValue(tokenCodes[arr[i]], i, 'tokenCodes[arr[i]]');
        }
