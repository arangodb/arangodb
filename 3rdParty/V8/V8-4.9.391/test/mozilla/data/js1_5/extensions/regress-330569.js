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
 * Contributor(s): antonglv
 *                 Andreas
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

var gTestfile = 'regress-330569.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 330569;
var summary = 'RegExp - throw InternalError on too complex regular expressions';
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
 
  var s;
  expect = 'InternalError: regular expression too complex';
 
  s = '<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">' +
    '<html>\n' +
    '<head>\n' +
    '<meta http-equiv="content-type" content="text/html; charset=windows-1250">\n' +
    '<meta name="generator" content="PSPad editor, www.pspad.com">\n' +
    '<title></title>\n'+
    '</head>\n' +
    '<body>\n' +
    '<!-- hello -->\n' +
    '<script language="JavaScript">\n' +
    'var s = document. body. innerHTML;\n' +
    'var d = s. replace (/<!--(.*|\n)*-->/, "");\n' +
    'alert (d);\n' +
    '</script>\n' +
    '</body>\n' +
    '</html>\n';

  options('relimit');

  try
  {
    /<!--(.*|\n)*-->/.exec(s);
  }
  catch(ex)
  {
    actual = ex + '';
  }

  reportCompare(expect, actual, summary + ': /<!--(.*|\\n)*-->/.exec(s)');

  function testre( re, n ) {
    for ( var i= 0; i <= n; ++i ) {
      re.test( Array( i+1 ).join() );
    }
  }

  try
  {
    testre( /(?:,*)*x/, 22 );
  }
  catch(ex)
  {
    actual = ex + '';
  }

  reportCompare(expect, actual, summary + ': testre( /(?:,*)*x/, 22 )');

  try
  {
    testre( /(?:,|,)*x/, 22 );
  }
  catch(ex)
  {
    actual = ex + '';
  }

  reportCompare(expect, actual, summary + ': testre( /(?:,|,)*x/, 22 )');

  try
  {
    testre( /(?:,|,|,|,|,)*x/, 10 );
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': testre( /(?:,|,|,|,|,)*x/, 10 )');

  exitFunc ('test');
}
