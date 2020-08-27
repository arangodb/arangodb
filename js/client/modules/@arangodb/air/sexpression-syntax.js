/**
 * S-expression parser
 *
 * Recursive descent parser of a simplified sub-set of s-expressions.
 *
 * NOTE: the format of the programs is used in the "Essentials of interpretation"
 * course: https://github.com/DmitrySoshnikov/Essentials-of-interpretation
 *
 * Grammar:
 *
 *   s-exp : atom
 *         | list
 *
 *   list : '(' list-entries ')'
 *
 *   list-entries : s-exp list-entries
 *                | ε
 *
 *   atom : symbol
 *        | number
 *
 * Examples:
 *
 *   (+ 10 5)
 *   > ['+', 10, 5]
 *
 *   (define (fact n)
 *     (if (= n 0)
 *       1
 *       (* n (fact (- n 1)))))
 *
 *   >
 *   ['define', ['fact', 'n'],
 *     ['if', ['=', 'n', 0],
 *       1,
 *       ['*', 'n', ['fact', ['-', 'n', 1]]]]]
 *
 * by Dmitry Soshnikov <dmitry.soshnikov@gmail.com>
 * MIT Style License, 2016
 */

'use strict';

/**
 * Parses a recursive s-expression into
 * equivalent Array representation in JS.
 */
const SExpressionParser = {
  parse(expression) {
    this._expression = expression;
    this._cursor = 0;
    this._ast = [];

    return this._parseExpression();
  },

  /**
   * s-exp : atom
   *       | list
   */
  _parseExpression() {
    this._whitespace();

    if (this._expression[this._cursor] === '(') {
      return this._parseList();
    }

    return this._parseAtom();
  },

  /**
   * list : '(' list-entries ')'
   */
  _parseList() {
    // Allocate a new (sub-)list.
    this._ast.push([]);

    this._expect('(');
    this._parseListEntries();
    this._expect(')');

    return this._ast[0];
  },

  /**
   * list-entries : s-exp list-entries
   *              | ε
   */
  _parseListEntries() {
    this._whitespace();

    // ε
    if (this._expression[this._cursor] === ')') {
      return [];
    }

    // s-exp list-entries

    let entry = this._parseExpression();

    if (entry !== '') {
      // Lists may contain nested sub-lists. In case we have parsed a nested
      // sub-list, it should be on top of the stack (see `_parseList` where we
      // allocate a list and push it onto the stack). In this case we don't
      // want to push the parsed entry to it (since it's itself), but instead
      // pop it, and push to previous (parent) entry.

      if (Array.isArray(entry)) {
        entry = this._ast.pop();
      }

      this._ast[this._ast.length - 1].push(entry);
    }

    return this._parseListEntries();
  },

  _readUntilTerminator(terminator) {
    let atom = '';
    while (this._expression[this._cursor] &&
           !terminator.test(this._expression[this._cursor])) {
      atom += this._expression[this._cursor];
      this._cursor++;
    }
    return atom;
  },
  /**
   * atom : symbol
   *      | number
   *      | string
   */
  _parseAtom() {
    if (this._expression[this._cursor] === '"') {
      // We're parsing a string
      this._cursor++;

      let atom = this._readUntilTerminator(/"/);

      return ["str", atom];
    } else {
      let atom = this._readUntilTerminator(/\s+|\)/);
      if (atom !== '' && !isNaN(atom)) {
        atom = Number(atom);
      }
      return atom;
    }
  },

  _whitespace() {
    const ws = /^\s+/;
    while (this._expression[this._cursor] &&
           ws.test(this._expression[this._cursor])) {
      this._cursor++;
    }
  },

  _expect(c) {
    if (this._expression[this._cursor] !== c) {
      throw new Error(
        `Unexpected token: ${this._expression[this._cursor]}, expected ${c}.`
      );
    }
    this._cursor++;
  }
};

// -----------------------------------------------------------------------------
// Tests

const assert = require('assert');

function test(actual, expected) {
  assert.deepEqual(SExpressionParser.parse(actual), expected);
}


function runTests() {
  // Empty lists.
  test(`()`, []);
  test(`( )`, []);
  test(`( ( ) )`, [ [ ] ]);

  // Simple atoms.
  test(`1`, 1);
  test(`foo`, 'foo');

  // Non-empty and nested lists.
  test(`(+ 1 15)`, ['+', 1, 15]);

  test(`(* (+ 1 15) (- 15 2))`, ['*', ['+', 1, 15], ['-', 15, 2]]);

  test(`
  (define (fact n)
    (if (= n 0)
      1
      (* n (fact (- n 1)))))`,

       ['define', ['fact', 'n'],
        ['if', ['=', 'n', 0],
         1,
         ['*', 'n', ['fact', ['-', 'n', 1]]]]]
      );

  test(
    `(define foo 'test')`,
    ['define', 'foo', `'test'`]
  );
}

// t is for transform
exports.t = SExpressionParser.parse;
