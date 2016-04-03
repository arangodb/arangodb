// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-8-12
description: >
    Allow reserved words as property names by set function within an
    object, accessed via indexing: const, export, import
---*/

        var test0 = 0, test1 = 1, test2 = 2;
        var tokenCodes  = {
            set const(value){
                test0 = value;
            },
            get const(){
                return test0;
            },
            set export(value){
                test1 = value;
            },
            get export(){
                return test1
            },
            set import(value){
                test2 = value;
            },
            get import(){
                return test2
            }
        }; 
        var arr = [
            'const',
            'export',
            'import'
        ];
        for (var i = 0; i < arr.length; i++) {
            assert.sameValue(tokenCodes[arr[i]], i, 'tokenCodes[arr[i]]');
        }
