// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-8-9
description: >
    Allow reserved words as property names by set function within an
    object, accessed via indexing: if, throw, delete
---*/

        var test0 = 0, test1 = 1, test2 = 2;
        var tokenCodes  = {
            set if(value){
                test0 = value;
            },
            get if(){
                return test0;
            },
            set throw(value){
                test1 = value;
            },
            get throw(){
                return test1
            },
            set delete(value){
                test2 = value;
            },
            get delete(){
                return test2;
            }
        }; 
        var arr = [
            'if', 
            'throw', 
            'delete'
        ];
        for (var i = 0; i < arr.length; i++) {
            assert.sameValue(tokenCodes[arr[i]], i, 'tokenCodes[arr[i]]');
        }
