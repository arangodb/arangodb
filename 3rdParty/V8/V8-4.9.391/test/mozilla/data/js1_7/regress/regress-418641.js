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
 * Contributor(s): Igor Bukanov
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

var gTestfile = 'regress-418641.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 418641;
var summary = '++ and -- correctness';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
function get_pre_check(operand, op)
{
  return "{\n"+
    "    "+operand+" = I;\n"+
    "    let tmp = "+op+op+operand+";\n"+
    "    if ("+operand+" !== Number(I) "+op+" 1)\n"+
    "        throw Error('"+op+op+operand+" case 1 for '+uneval(I));\n"+
    "    if (tmp !== "+operand+")\n"+
    "        throw Error('"+op+op+operand+" case 2 for '+uneval(I));\n"+
    "}\n";    
}

function get_post_check(operand, op)
{
  return "{\n"+
    "    "+operand+" = I;\n"+
    "    let tmp = "+operand+op+op+";\n"+
    "    if ("+operand+" !== Number(I) "+op+" 1)\n"+
    "        throw Error('"+operand+op+op+" case 1 for '+uneval(I));\n"+
    "    if (tmp !== Number(I))\n"+
    "        throw Error('"+op+op+operand+" case 2 for '+uneval(I));\n"+
    "}\n";    
}

function get_check_source(operand)
{
  return get_pre_check(operand, '+')+
    get_pre_check(operand, '-')+
    get_post_check(operand, '+')+
    get_post_check(operand, '-');
}

var arg_check = Function('I', 'a', get_check_source('a'));
var let_check = Function('I', 'let a;'+get_check_source('a'));
var var_check = Function('I', 'var a;'+get_check_source('a'));

var my_name;
var my_obj = {};
var my_index = 0;
var name_check = Function('I', get_check_source('my_name'));
var prop_check = Function('I', get_check_source('my_obj.x'));
var elem_check = Function('I', get_check_source('my_obj[my_index]'));

var test_values = [0 , 0.5, -0.0, (1 << 30) - 1, 1 - (1 << 30)];

for (let i = 0; i != test_values.length; i = i + 1) {
  let x = [test_values[i], String(test_values[i])];
  for (let j = 0; j != x.length; j = j + 1) {
    try
    {
      expect = actual = 'No Error';
      let test_value = x[j];
      arg_check(test_value, 0);
      let_check(test_value);
      var_check(test_value);
      name_check(test_value);
      prop_check(test_value);
      elem_check(test_value);
    }
    catch(ex)
    {
      actual = ex + '';
    }
    reportCompare(expect, actual, summary + ': ' + i + ', ' + j);
  }
}
