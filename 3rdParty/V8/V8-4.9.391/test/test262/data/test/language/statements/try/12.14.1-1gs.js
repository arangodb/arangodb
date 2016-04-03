// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 12.14.1-1gs
description: >
    Strict Mode - SyntaxError is thrown if a TryStatement with a Catch
    occurs within strict code and the Identifier of the Catch
    production is eval
negative: SyntaxError
flags: [onlyStrict]
---*/

throw NotEarlyError;
try { } catch (eval) { }
