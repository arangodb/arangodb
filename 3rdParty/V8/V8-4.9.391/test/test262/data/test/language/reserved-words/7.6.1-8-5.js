// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-8-5
description: >
    Allow reserved words as property names by set function within an
    object, accessed via indexing: finally, return, void
---*/

        var test0 = 0, test1 = 1, test2 = 2;
        var tokenCodes  = {
            set finally(value){
                test0 = value;
            },
            get finally(){
                return test0;
            },
            set return(value){
                test1 = value;
            },
            get return(){
                return test1;
            },
            set void(value){
                test2 = value;
            },
            get void(){
                return test2;
            }
        }; 
        var arr = [
            'finally', 
            'return', 
            'void'
        ];
        for (var i = 0; i < arr.length; i++) {
            assert.sameValue(tokenCodes[arr[i]], i, 'tokenCodes[arr[i]]');
        }
