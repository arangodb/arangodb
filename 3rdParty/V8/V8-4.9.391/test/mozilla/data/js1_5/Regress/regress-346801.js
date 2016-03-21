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
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Jesse Ruderman
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

var gTestfile = 'regress-346801.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 346801;
var summary = 'Hang regression from bug 346021';
var actual = '';
var expect = 'No Hang';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  try
  {
    var Class = {
      create: function() {
        return function() {
          this.initialize.apply(this, arguments);
        }
      }
    }

    Object.extend = function(destination, source) {
      print("Start");
//      print(destination);
//      print(source);
      if(destination==source)
        print("Same desination and source!");
      var i = 0;
      for (property in source) {
//        print("  " + property);
        destination[property] = source[property];
        ++i;
        if (i > 1000) {
          throw "Hang";
        }
      }
      print("Finish");
      return destination;
    }

    var Ajax = {
    };

    Ajax.Base = function() {};
    Ajax.Base.prototype = {
      responseIsFailure: function() { }
    }

    Ajax.Request = Class.create();

    Ajax.Request.prototype = Object.extend(new Ajax.Base(), {});

    Ajax.Updater = Class.create();

    Object.extend(Ajax.Updater.prototype, Ajax.Request.prototype);
    actual = 'No Hang';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
