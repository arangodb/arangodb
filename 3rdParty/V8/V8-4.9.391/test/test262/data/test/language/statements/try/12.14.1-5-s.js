// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 12.14.1-5-s
description: >
    Strict Mode - SyntaxError isn't thrown if a TryStatement with a
    Catch occurs within strict code and the Identifier of the Catch
    production is Arguments
---*/

var isInstance = false;

        try {
            throw new Error("...");
        } catch (Arguments) {
            isInstance = Arguments instanceof Error;
        }

assert(isInstance);
