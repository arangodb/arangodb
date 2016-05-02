/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Igor Bukanov
 *   Rob Ginda rginda@netscape.com
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

gTestsuite = 'e4x';

if (typeof version != 'undefined') {
    // At least in Rhino, if version is set to 150 (which is what is used
    // in top level shell.js), the implementation assumes E4X is not available.
    version(160);
}

/*
 * Report a failure in the 'accepted' manner
 */
function reportFailure (section, msg)
{
    msg = inSection(section)+"\n"+msg;
    var lines = msg.split ("\n");
    for (var i=0; i<lines.length; i++)
        print (FAILED + lines[i]);
}

function START(summary)
{
    SUMMARY = summary;
    printStatus(summary);
}

function TEST(section, expected, actual)
{
    var expected_t = typeof expected;
    var actual_t = typeof actual;
    var output = "";
   
    SECTION = section;
    EXPECTED = expected;
    ACTUAL = actual;

    return reportCompare(expected, actual, inSection(section) + SUMMARY);
}

function TEST_XML(section, expected, actual)
{
    var actual_t = typeof actual;
    var expected_t = typeof expected;

    SECTION = section;
    EXPECTED = expected;
    ACTUAL = actual;

    if (actual_t != "xml") {
        // force error on type mismatch
        return TEST(section, new XML(), actual);
    }
 
    if (expected_t == "string") {
        return TEST(section, expected, actual.toXMLString());
    }

    if (expected_t == "number") {
        return TEST(section, String(expected), actual.toXMLString());
    }
 
    throw section + ": Bad TEST_XML usage: type of expected is " +
        expected_t + ", should be number or string";

    // suppress warning
    return false;
}

function SHOULD_THROW(section)
{
    TEST(section, "exception", "no exception");
}

function END()
{
}

if (typeof options == 'function')
{
  options('xml');
}
