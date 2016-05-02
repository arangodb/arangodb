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
 * Netscape Communication Corporation.
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

var include = document.location.href.indexOf('?include') != -1;
document.write( '<title>Import ' + (include ? 'Include' : 'Exclude') + ' Test List<\/title>')

function doImport()
{
  var lines =
    document.forms["foo"].elements["testList"].value.split(/\r?\n/);
  var suites = window.opener.suites;
  var elems = window.opener.document.forms["testCases"].elements;

  if (include && document.forms["foo"].elements["clear_all"].checked)
    window.opener._selectNone();

  for (var l in lines)
  {
    if (!lines[l])
    {
      continue;
    }

    if (lines[l].search(/^\s$|\s*\#/) == -1)
    {
      var ary = lines[l].match (/(.*)\/(.*)\/(.*)/);

      if (!ary)
      {
        if (!confirm ("Line " + l + " '" +
                      lines[l] + "' is confusing, " +
                      "continue with import?"))
        {
          return;
        }
      }
                 
      if (suites[ary[1]] &&
          suites[ary[1]].testDirs[ary[2]] &&
          suites[ary[1]].testDirs[ary[2]].tests[ary[3]])
      {
        var radioname = suites[ary[1]].testDirs[ary[2]].tests[ary[3]].id;
        var radio = elems[radioname];
        if (include && !radio.checked)
        {
          radio.checked = true; 
          window.opener.onRadioClick(radio);
        }
        else if  (!include && radio.checked)
        {
          radio.checked = false; 
          window.opener.onRadioClick(radio);
        }
      }
    }
  }

  setTimeout('window.close();', 200);
                 
}
