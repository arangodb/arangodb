/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is JavaScript Engine testing utilities.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Brendan
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var gTestfile = 'regress-380237-01.js';

//-----------------------------------------------------------------------------
var BUGNUMBER = 380237;
var summary = 'Generator expressions - sudoku';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

if (this.version) version(180);

// XXX should be standard (and named clone, after Java?)
Object.prototype.copy = function () {
    let o = {}
    for (let i in this)
        o[i] = this[i]
    return o
}

// Make arrays and strings act more like Python lists by iterating their values, not their keys.
Array.prototype.__iterator__ = String.prototype.__iterator__ = function () {
    for (let i = 0; i < this.length; i++)
        yield this[i]
}

// Containment testing for arrays and strings that should be coherent with their __iterator__.
Array.prototype.contains = String.prototype.contains = function (e) {
    return this.indexOf(e) != -1
}

Array.prototype.repeat = String.prototype.repeat = function (n) {
    let s = this.constructor()
    for (let i = 0; i < n; i++)
        s = s.concat(this)
    return s
}

String.prototype.center = function (w) {
    let n = this.length
    if (w <= n)
        return this
    let m = Math.floor((w - n) / 2)
    return ' '.repeat(m) + this + ' '.repeat(w - n - m)
}

Array.prototype.toString = Array.prototype.toSource
Object.prototype.toString = Object.prototype.toSource

// XXX thought spurred by the next two functions: array extras should default to identity function

function all(seq) {
    for (let e in seq)
        if (!e)
            return false
    return true
}

function some(seq) {
    for (let e in seq)
        if (e)
            return e
    return false
}

function cross(A, B) {
    return [a+b for (a in A) for (b in B)]
}

function dict(A) {
    let d = {}
    for (let e in A)
        d[e[0]] = e[1]
    return d
}

function set(A) {
    let s = []
    for (let e in A)
        if (!s.contains(e))
            s.push(e)
    return s
}

function zip(A, B) {
    let z = []
    let n = Math.min(A.length, B.length)
    for (let i = 0; i < n; i++)
        z.push([A[i], B[i]])
    return z
}

rows = 'ABCDEFGHI'
cols = '123456789'
digits   = '123456789'
squares  = cross(rows, cols)
unitlist = [cross(rows, c) for (c in cols)]
           .concat([cross(r, cols) for (r in rows)])
           .concat([cross(rs, cs) for (rs in ['ABC','DEF','GHI']) for (cs in ['123','456','789'])])
units = dict([s, [u for (u in unitlist) if (u.contains(s))]] 
             for (s in squares))
peers = dict([s, set([s2 for (u in units[s]) for (s2 in u) if (s2 != s)])]
             for (s in squares))

// Given a string of 81 digits (or . or 0 or -), return a dict of {cell:values}.
function parse_grid(grid) {
    grid = [c for (c in grid) if ('0.-123456789'.contains(c))]
    let values = dict([s, digits] for (s in squares))

    for (let [s, d] in zip(squares, grid))
        if (digits.contains(d) && !assign(values, s, d))
            return false
    return values
}

// Eliminate all the other values (except d) from values[s] and propagate.
function assign(values, s, d) {
    if (all(eliminate(values, s, d2) for (d2 in values[s]) if (d2 != d)))
        return values
    return false
}

// Eliminate d from values[s]; propagate when values or places <= 2.
function eliminate(values, s, d) {
    if (!values[s].contains(d))
        return values // Already eliminated
    values[s] = values[s].replace(d, '')
    if (values[s].length == 0)
	return false  // Contradiction: removed last value
    if (values[s].length == 1) {
	// If there is only one value (d2) left in square, remove it from peers
        let d2 = values[s][0]
        if (!all(eliminate(values, s2, d2) for (s2 in peers[s])))
            return false
    }
    // Now check the places where d appears in the units of s
    for (let u in units[s]) {
	let dplaces = [s for (s in u) if (values[s].contains(d))]
	if (dplaces.length == 0)
	    return false
	if (dplaces.length == 1)
	    // d can only be in one place in unit; assign it there
            if (!assign(values, dplaces[0], d))
                return false
    }
    return values
}

// Used for debugging.
function print_board(values) {
    let width = 1 + Math.max.apply(Math, [values[s].length for (s in squares)])
    let line = '\n' + ['-'.repeat(width*3)].repeat(3).join('+')
    for (let r in rows)
        print([values[r+c].center(width) + ('36'.contains(c) && '|' || '')
               for (c in cols)].join('') + ('CF'.contains(r) && line || ''))
    print('\n')
}

easy = "..3.2.6..9..3.5..1..18.64....81.29..7.......8..67.82....26.95..8..2.3..9..5.1.3.."

print_board(parse_grid(easy))

// Using depth-first search and constraint propagation, try all possible values.
function search(values) {
    if (!values)
        return false    // Failed earlier
    if (all(values[s].length == 1 for (s in squares))) 
        return values   // Solved!

    // Choose the unfilled square s with the fewest possibilities
    // XXX Math.min etc. should work with generator expressions and other iterators
    // XXX Math.min etc. should work on arrays (lists or tuples in Python) as well as numbers
    let a = [values[s].length + s for (s in squares) if (values[s].length > 1)].sort()
    let s = a[0].slice(-2)

    return some(search(assign(values.copy(), s, d)) for (d in values[s]))
}

hard = '4.....8.5.3..........7......2.....6.....8.4......1.......6.3.7.5..2.....1.4......'

print_board(search(parse_grid(hard)))

  delete Object.prototype.copy;
 
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
