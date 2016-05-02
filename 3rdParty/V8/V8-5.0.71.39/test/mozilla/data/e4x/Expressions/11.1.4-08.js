/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

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
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Biju
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

gTestfile = '11.1.4-08.js';

var summary = "11.1.4 - XML Initializer - {} Expressions - 08";

var BUGNUMBER = 325750;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);
printStatus('E4X: inconsistencies in the use of {} syntax Part Deux');

// https://bugzilla.mozilla.org/show_bug.cgi?id=318922
// https://bugzilla.mozilla.org/show_bug.cgi?id=321549

var exprs = [];
var iexpr;

exprs.push({expr: 'b=\'\\\'\';\na=<a>\n  <b c=\'c{b}>x</b>\n</a>;', valid: false});
exprs.push({expr: 'b=\'\\\'\';\na=<a>\n  <b c={b}c\'>x</b>\n</a>;', valid: false});
exprs.push({expr: 'b=\'b\';\na=<a xmlns:p=\'http://a.uri/\'>\n  <p:b{b}>x</p:bb>\n</a>;', valid: true});
exprs.push({expr: 'b=\'b\';\na=<a xmlns:p=\'http://a.uri/\'>\n  <p:b{b}>x</p:bb>\n</a>;', valid: true});
exprs.push({expr: 'b=\'b\';\na=<a xmlns:p=\'http://a.uri/\'>\n  <p:{b}b>x</p:bb>\n</a>;', valid: true});
exprs.push({expr: 'b=\'b\';\na=<a xmlns:p=\'http://a.uri/\'>\n  <p:{b}b>x</p:bb>\n</a>;', valid: true});
exprs.push({expr: 'b=\'b\';\na=<a xmlns:p=\'http://a.uri/\'>\n  <{b}b>x</bb>\n</a>;', valid: true});
exprs.push({expr: 'b=\'b\';\na=<a>\n  <b{b}>x</bb>\n</a>;', valid: true});
exprs.push({expr: 'b=\'b\';\na=<a>\n  <{b+\'b\'}>x</bb>\n</a>;', valid: true});
exprs.push({expr: 'b=\'b\';\na=<a>\n  <{b}b>x</bb>\n</a>;', valid: true});
exprs.push({expr: 'b=\'c\';\na=<a>\n  <b c=\'c\'{b}>x</b>\n</a>;', valid: false});
exprs.push({expr: 'b=\'c\';\na=<a>\n  <b c={b}>x</b>\n</a>;', valid: true});
exprs.push({expr: 'b=\'c\';\na=<a>\n  <b c={b}\'c\'>x</b>\n</a>;', valid: false});
exprs.push({expr: 'b=\'c\';\na=<a>\n  <b c{b}=\'c\'>x</b>\n</a>;', valid: true});
exprs.push({expr: 'b=\'c\';\na=<a>\n  <b {b+\'c\'}=\'c\'>x</b>\n</a>;', valid: true});
exprs.push({expr: 'b=\'c\';\na=<a>\n  <b {b}=\'c\'>x</b>\n</a>;', valid: true});
exprs.push({expr: 'b=\'c\';\na=<a>\n  <b {b}c=\'c\'>x</b>\n</a>;', valid: true});
exprs.push({expr: 'm=1;\na=<a>\n  x {m} z\n</a>;', valid: true});
exprs.push({expr: 'm=1;\na=new XML(m);', valid: true});
exprs.push({expr: 'm=<m><n>o</n></m>;\na=<a>\n  <b>x {m} z</b>\n</a>;', valid: true});
exprs.push({expr: 'm=<m><n>o</n></m>;\na=<a>\n  <{m}>x  z</{m}>\n</a>;', valid: false});
exprs.push({expr: 'm=<m>o</m>;\na=<a>\n  <{m}>x  z</{m}>\n</a>;', valid: true});
exprs.push({expr: 'm=[1,\'x\'];\na=<a>\n  x {m} z\n</a>;', valid: true});
exprs.push({expr: 'm=[1,\'x\'];\na=new XML(m);', valid: false});
exprs.push({expr: 'm=[1];\na=new XML(m);', valid: false});
exprs.push({expr: 'm=\'<m><n>o</n></m>\';\na=<a>\n  <b>x {m} z</b>\n</a>;', valid: true});
exprs.push({expr: 'm=\'<m><n>o</n></m>\';\na=<a>\n  x {m} z\n</a>;', valid: true});
exprs.push({expr: 'm=\'x\';\na=new XML(m);', valid: true});
exprs.push({expr: 'm=new Date();\na=new XML(m);', valid: false});
exprs.push({expr: 'm=new Number(\'1\');\na=new XML(m);', valid: true});
exprs.push({expr: 'm=new String(\'x\');\na=new XML(\'<a>\\n  {m}\\n</a>\');', valid: true});
exprs.push({expr: 'm=new String(\'x\');\na=new XML(m);', valid: true});
exprs.push({expr: 'm={a:1,b:\'x\'};\na=<a>\n  x {m} z\n</a>;', valid: true});
exprs.push({expr: 'm={a:1,b:\'x\'};\na=new XML(m);', valid: false});
exprs.push({expr: 'p="p";\nu=\'http://a.uri/\';\na=<a xmlns:p{p}={\'x\',1,u}>\n  <pp:b>x</pp:b>\n</a>;', valid: true});
exprs.push({expr: 'p="p";\nu=\'http://a.uri/\';\na=<a xmlns:{p}p={\'x\',1,u}>\n  <pp:b>x</pp:b>\n</a>;', valid: true});
exprs.push({expr: 'p="pp";\nu=\'http://a.uri/\';\na=<a xmlns:{p}={\'x\',1,u}>\n  <pp:b>x</pp:b>\n</a>;', valid: true});
exprs.push({expr: 'u=\'http://a.uri/\';\na=<a xmlns:p={(function(){return u})()}>\n  <p:b>x</p:b>\n</a>;', valid: true});
exprs.push({expr: 'u=\'http://a.uri/\';\na=<a xmlns:p={\'x\',1,u}>\n  <p:b>x</p:b>\n</a>;', valid: true});
exprs.push({expr: 'u=\'http://a.uri/\';\na=<a xmlns:p={u}>\n  <{u}:b>x</p:b>\n</a>;', valid: false});
exprs.push({expr: 'u=\'http://a.uri/\';\na=<a xmlns:p={var d=2,u}>\n  <p:b>x</p:b>\n</a>;', valid: false});
exprs.push({expr: 'u=\'http://a.uri/\';\nn=new Namespace("p",u);\na=<a xmlns:p={n}>\n  <{u}:b>x</p:b>\n</a>;', valid: false});
exprs.push({expr: 'u=\'http://a.uri/\';\nn=new Namespace("p",u);\na=<a xmlns:p={u}>\n  <{u}:b>x</p:b>\n</a>;', valid: false});
exprs.push({expr: 'u=\'http://a.uri/\';\nn=new Namespace("p",u);\na=<a xmlns:{n}={n}>\n  <p:b>x</p:b>\n</a>;', valid: false});
exprs.push({expr: 'u=\'http://a.uri/\';\nn=new Namespace("p",u);\na=<a xmlns:{n}={n}>\n  <{n}:b>x</p:b>\n</a>;', valid: false});
exprs.push({expr: 'u=\'http://a.uri/\';\nn=new Namespace(u);\na=<a xmlns:p={n}>\n  <p:b>x</p:b>\n</a>;', valid: true});
exprs.push({expr: 'u=\'http://a.uri/\';\nn=new Namespace(u);\na=<a xmlns:p={n}>\n  <{n}:b>x</p:b>\n</a>;', valid: false});
exprs.push({expr: 'u=\'http://a.uri/\';\nn=new Namespace(u);\na=<a xmlns:p={u}>\n  <p:b>x</p:b>\n</a>;', valid: true});
exprs.push({expr: 'u=\'http://a.uri/\';\nn=new Namespace(u);\na=<a xmlns:p={u}>\n  <{n}:b>x</p:b>\n</a>;', valid: false});
exprs.push({expr: 'u=\'http://a.uri/\';\nn=new Namespace(u);\na=<a xmlns:p={u}>\n  <{n}:b>x</p:b>\n</a>;', valid: false});
exprs.push({expr: 'u=\'http://a.uri/\';\np=\'p\';\na=<a xmlns:p={u}>\n  <{p}:b>x</p:b>\n</a>;', valid: true});
exprs.push({expr: 'u=\'http://a.uri/\';\np=\'p\';\na=<a xmlns:pp={u}>\n  <p{p}:b>x</pp:b>\n</a>;', valid: true});
exprs.push({expr: 'u=\'http://a.uri/\';\np=\'p\';\na=<a xmlns:pp={u}>\n  <{p}p:b>x</pp:b>\n</a>;', valid: true});
exprs.push({expr: 'u=\'http://a.uri/\';\np=\'p\';\nns="ns";\na=<a xml{ns}:pp={u}>\n  <{p}p:b>x</pp:b>\n</a>;', valid: true});
exprs.push({expr: 'u=\'http://a.uri/\';\np=\'p\';\nns="xmlns";\na=<a {ns}:pp={u}>\n  <{p}p:b>x</pp:b>\n</a>;', valid: true});
exprs.push({expr: 'u=\'http://a.uri/\';\np=\'p\';\nxml="xml";\na=<a {xml}ns:pp={u}>\n  <{p}p:b>x</pp:b>\n</a>;', valid: true});
exprs.push({expr: 'u=\'http://a.uri/\';\np=\'pp\';\na=<a xmlns:pp={u}>\n  <{p}:b>x</pp:b>\n</a>;', valid: true});
exprs.push({expr: 'u=\'http://a.uri/\';\nu2=\'http://uri2.sameprefix/\';\nn=new Namespace(\'p\',u2);\na=<a xmlns:p={u}>\n  <{n}:b>x</p:b>\n</a>;', valid: false}); // This should always fail

for (iexpr = 0; iexpr < exprs.length; ++iexpr)
{
  evalStr(exprs, iexpr);
}

END();

function evalStr(exprs, iexpr)
{
  var value;
  var valid;
  var passfail;
  var obj = exprs[iexpr];

  try
  {
    value = eval(obj.expr).toXMLString();
    valid = true;
  }
  catch(ex)
  {
    value = ex + '';
    valid = false;
  }

  passfail = (valid === obj.valid);

  msg = iexpr + ': ' + (passfail ? 'PASS':'FAIL') +
        ' expected: ' + (obj.valid ? 'valid':'invalid') +
        ', actual: ' + (valid ? 'valid':'invalid') + '\n' +
        'input: ' + '\n' +
        obj.expr + '\n' +
        'output: ' + '\n' +
        value + '\n\n';
 
  printStatus(msg);

  TEST(iexpr, obj.valid, valid);

  return passfail;
}


