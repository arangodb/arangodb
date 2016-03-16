// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-8-10
description: >
    Allow reserved words as property names by set function within an
    object, accessed via indexing: in, try, class
---*/

        var test0 = 0, test1 = 1, test2 = 2;
        var tokenCodes  = {
            set in(value){
                test0 = value;
            },
            get in(){
                return test0;
            },
            set try(value){
                test1 = value;
            },
            get try(){
                return test1
            },
            set class(value){
                test2 = value;
            },
            get class(){
                return test2;
            }
        }; 
        var arr = [
            'in', 
            'try',
            'class'
        ];
        for (var i = 0; i < arr.length; i++) {
            assert.sameValue(tokenCodes[arr[i]], i, 'tokenCodes[arr[i]]');
        }
