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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

gTestfile = 'script-001.js';

/**
   File Name:          script-001.js
   Section:
   Description:        new NativeScript object


   js> parseInt(123,"hi")
   123
   js> parseInt(123, "blah")
   123
   js> s
   js: s is not defined
   js> s = new Script

   undefined;


   js> s = new Script()

   undefined;


   js> s.getJSClass
   js> s.getJSClass = Object.prototype.toString
   function toString() {
   [native code]
   }

   js> s.getJSClass()
   [object Script]
   js> s.compile( "return 3+4" )
   js: JavaScript exception: javax.javascript.EvaluatorException: "<Scr
   js> s.compile( "3+4" )

   3 + 4;


   js> typeof s
   function
   js> s()
   Jit failure!
   invalid opcode: 1
   Jit Pass1 Failure!
   javax/javascript/gen/c13 initScript (Ljavax/javascript/Scriptable;)V
   An internal JIT error has occurred.  Please report this with .class
   jit-bugs@itools.symantec.com

   7
   js> s.compile("3+4")

   3 + 4;


   js> s()
   Jit failure!
   invalid opcode: 1
   Jit Pass1 Failure!
   javax/javascript/gen/c17 initScript (Ljavax/javascript/Scriptable;)V
   An internal JIT error has occurred.  Please report this with .class
   jit-bugs@itools.symantec.com

   7
   js> quit()

   C:\src\ns_priv\js\tests\ecma>shell

   C:\src\ns_priv\js\tests\ecma>java -classpath c:\cafe\java\JavaScope;
   :\src\ns_priv\js\tests javax.javascript.examples.Shell
   Symantec Java! JustInTime Compiler Version 210.054 for JDK 1.1.2
   Copyright (C) 1996-97 Symantec Corporation

   js> s = new Script("3+4")

   3 + 4;


   js> s()
   7
   js> s2 = new Script();

   undefined;


   js> s.compile( "3+4")

   3 + 4;


   js> s()
   Jit failure!
   invalid opcode: 1
   Jit Pass1 Failure!
   javax/javascript/gen/c7 initScript (Ljavax/javascript/Scriptable;)V
   An internal JIT error has occurred.  Please report this with .class
   jit-bugs@itools.symantec.com

   7
   js> quit()
   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "script-001";
var VERSION = "JS1_3";
var TITLE   = "NativeScript";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

if (typeof Script == 'undefined')
{
  print('Test skipped. Script not defined.');
  new TestCase( SECTION,
                "var s = new Script(); typeof s",
                "Script not supported, test skipped.",
                "Script not supported, test skipped." );
}
else
{
  var s = new Script();
  s.getJSClass = Object.prototype.toString;

  new TestCase( SECTION,
                "var s = new Script(); typeof s",
                "function",
                typeof s );

  new TestCase( SECTION,
                "s.getJSClass()",
                "[object Script]",
                s.getJSClass() );
}

test();
