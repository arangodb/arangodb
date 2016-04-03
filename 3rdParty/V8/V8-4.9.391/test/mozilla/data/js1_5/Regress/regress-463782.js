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
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Gavin Sharp
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

var gTestfile = 'regress-463782.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 463782;
var summary = 'Do not assert: "need a way to EOT now, since this is trace end": 0';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

jit(true);

function dateCheck() {
  return true;
}
function dateToString()
{
  if (!this.dtsReturnValue)
    this.dtsReturnValue = "200811080616";
  return this.dtsReturnValue
    }
      
function placeAd2() {
  var adClasses = {
    "": {
    templateCheck: function () {
        var foo = ({
          allianz:{
            where:["intl/turningpoints"],
                when:["200805010000/200901010000"],
                what:["!234x60", "!bigbox_2", "!leaderboard_2", "!88x31"]
                },
              trendMicro:{
            where:["techbiz/tech/threatmeter"],
                when:["200806110000/200812310000"],
                what:["leaderboard"]
                },
              rolex_bb:{
            where:["politics/transitions"],
                when:["200811050000/200901312359"],
                what:["!bigbox"]
                }
          });
        
        for (a in foo) {
          if (dateCheck("", dateToString())) {
            for (var c = 0; c < 1; c++) {
            }
          }
        }
        return true;
      }
    }
  };
      
  adClasses[""].templateCheck();
}

placeAd2();

jit(false);

reportCompare(expect, actual, summary);

