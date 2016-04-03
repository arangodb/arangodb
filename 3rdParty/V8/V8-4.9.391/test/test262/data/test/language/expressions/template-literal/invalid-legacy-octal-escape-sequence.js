// Copyright (C) 2014 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 16.1
description: Invalid octal escape sequence
info: >
    TemplateCharacter (11.8.6) must not be extended to include
    LegacyOctalEscapeSequence as defined in B.1.2.
negative: SyntaxError
---*/

`\00`;
