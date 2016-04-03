// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-8-13
description: >
    Allow reserved words as property names by set function within an
    object, accessed via indexing: implements, let, private
---*/

        var test0 = 0, test1 = 1, test2 = 2;
        var tokenCodes  = {
            set implements(value){
                test0 = value;
            },
            get implements(){
                return test0;
            },
            set let(value){
                test1 = value;
            },
            get let(){
                return test1
            },
            set private(value){
                test2 = value;
            },
            get private(){
                return test2;
            }
        }; 
        var arr = [
            'implements',
            'let',
            'private'
        ];
        for (var i = 0; i < arr.length; i++) {
            assert.sameValue(tokenCodes[arr[i]], i, 'tokenCodes[arr[i]]');
        }
