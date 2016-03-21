// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-8-2
description: >
    Allow reserved words as property names by set function within an
    object, accessed via indexing: break, case, do
---*/

        var test0 = 0, test1 = 1, test2 = 2;
        var tokenCodes  = {
            set break(value){
                test0 = value;        
            },
            get break(){
                return test0;
            },
            set case(value){
                test1 = value;
            },
            get case(){
                return test1;
            },
            set do(value){
                test2 = value;
            },
            get do(){
                return test2;
            }
        }; 
        var arr = [
            'break',
            'case',
            'do'
        ];
        for (var i = 0; i < arr.length; i++) {
            assert.sameValue(tokenCodes[arr[i]], i, 'tokenCodes[arr[i]]');
        }
