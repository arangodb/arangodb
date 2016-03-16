// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-8-8
description: >
    Allow reserved words as property names by set function within an
    object, accessed via indexing: this, with, default
---*/

        var test0 = 0, test1 = 1, test2 = 2;
        var tokenCodes  = {
            set this(value){
                test0 = value;
            },
            get this(){
                return test0;
            },
            set with(value){
                test1 = value;
            },
            get with(){
                return test1;
            },
            set default(value){
                test2 = value;
            },
            get default(){
                return test2;
            }
        }; 
        var arr = [
            'this', 
            'with', 
            'default'
        ];
        for (var i = 0; i < arr.length; i++) {
            assert.sameValue(tokenCodes[arr[i]], i, 'tokenCodes[arr[i]]');
        }
