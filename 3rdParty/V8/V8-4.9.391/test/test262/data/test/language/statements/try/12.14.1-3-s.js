// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 12.14.1-3-s
description: >
    Strict Mode - SyntaxError isn't thrown if a TryStatement with a
    Catch occurs within strict code and the Identifier of the Catch
    production is EVAL but throws SyntaxError if it is eval
flags: [onlyStrict]
---*/

assert.throws(SyntaxError, function() {
 eval(" try { \
             throw new Error(\"...\");\
             return false;\
         } catch (EVAL) {\
             try\
             {\
               throw new Error(\"...\");\
             }catch(eval)\
             {\
                 return EVAL instanceof Error;\
              }\
         }");
});
