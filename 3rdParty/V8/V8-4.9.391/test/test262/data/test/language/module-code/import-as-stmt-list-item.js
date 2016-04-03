// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 15.2
description: >
    An ImportDeclaration is not a valid StatementListItem and is therefore
    restricted from appearing within statements in a ModuleBody.
flags: [module]
negative: SyntaxError
---*/

{
  import { x } from 'y';
}
