// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Function declarations in global or function scope are {DontDelete}
es5id: 13_A12_T2
description: >
    Checking if deleting a function that is declared in function scope
    fails
flags: [noStrict]
---*/

ALIVE="Letov is alive"

function __cont(){

    function __func(){
        return ALIVE;
    };
    
    //////////////////////////////////////////////////////////////////////////////
    //CHECK#1
    if (delete __func) {
    	$ERROR('#1: delete __func returning false');
    }
    //
    //////////////////////////////////////////////////////////////////////////////
    
    //////////////////////////////////////////////////////////////////////////////
    //CHECK#2
    if (__func() !== ALIVE) {
    	$ERROR('#2: __func() === ALIVE. Actual: __func() ==='+__func());
    }
    //
    //////////////////////////////////////////////////////////////////////////////
};

__cont();
