// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Heiko Kernbach
// / @author Lars Maier
// / @author Markus Pfeiffer
// / @author Copyright 2020, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

/*

  Demo translating JavaScript syntax into AIR.

  Beware that this is *syntactic* and we make no promises
  about semantics or anything

  This might be risky because users expect something that looks like javascript
  to behave like javascript.

 */

//
// The acorn parser seems to output something that's "standardised",
// namely "estree", though it seems entirely obscure to me what the
// *definition* of estree is.
//
// here are some links:
//
// - https://github.com/estree/estree
// - https://developer.mozilla.org/en-US/docs/Mozilla/Projects/SpiderMonkey/Parser_API
//
// Our translator will just walk the AST produced by the Acorn parser
// and spit out AIR that *should* be interpretable by Greenspun
//
const {Parser} = require("acorn");


const expressionDispatch = {
  'Literal': function(expr) {
    // TODO translate objects, arrays?
    return expr.value;
  },
  'CallExpression': function(expr) {
    return [toAir(expr.callee)].concat(expr.arguments.map(toAir));
  }
};

const dispatch = {
  'Program': function(node) {
    return node.body.map(toAir);
  },
  'ExpressionStatement': function(node) {
    console.log(JSON.stringify(node, null, 4));
    const exprType = node.expression.type;
    if (exprType in expressionDispatch) {
      // TODO: what about objects?
      return expressionDispatch[exprType](node.expression);
    } else {
      throw "Don't know how to process an expression of type " + exprType + " " + JSON.stringify(node, null, 4);
    }
    return [];
  },
  'IfStatement': function(node) {
    return ["if",
            [toAir(node.test),
             toAir(node.consequent)],
            [true,
             toAir(node.alternate)]];
  },
  'BinaryExpression': function(node) {
    return [node.operator, toAir(node.left), toAir(node.right)];
  },
  'Identifier': function(node) {
    return node.name;
  },
  'BlockStatement': function(node) {
    return ["seq"].concat(node.body.map(toAir));
  },
  'FunctionDeclaration': function(node) {
    return toAir(node.body);
  },
  'ReturnStatement': function(node) {
    return ["imperative-returns-suck", toAir(node.argument)];
  },
  'Literal': function(node) {
    return node.value;
  },
  'ForInStatement': function(node) {
    return ["for-each", [], toAir(node.body)];
  },
  'CallExpression': function(node) {
    return [toAir(node.callee)].concat(node.arguments.map(toAir));
  }

};

function toAir(astNode) {
  let nodeType = astNode.type;

  if (nodeType in dispatch) {
    return dispatch[astNode.type](astNode);
  } else {
    throw "Don't know how to transform node of type " + nodeType + " " + JSON.stringify(astNode, null, 4);
  }
}

function codeToAst(code) {
  return Parser.parse(code, { ecmaVersion: 2020});
}

function codeToAir(code) {
  return toAir(codeToAst(code));
}

console.log(JSON.stringify(codeToAir(`
function hello(a, b) {
  if (a == b) {
    return 15;
  } else {
    return 16;
  }
}`),
null, 4));

console.log(JSON.stringify(codeToAir(`
function hello(a, b) {
  var x = accumRef("globalAccum");
  for (var i in a) {
    palaber(i, b);
  }
}`),
null, 4));

