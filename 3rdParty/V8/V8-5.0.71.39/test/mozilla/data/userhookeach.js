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
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Bob Clary
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

/*
 * Spider hook function to check if an individual browser based JS test
 * has completed executing.
 */

var gCheckInterval = 1000;
var gCurrentTestId;
var gReport;
var gCurrentTestStart;
var gCurrentTestStop;
var gCurrentTestValid;
var gPageStart;
var gPageStop;

function userOnStart()
{
  try
  {
    dlog('userOnStart');
    registerDialogCloser();
  }
  catch(ex)
  {
    cdump('Spider: FATAL ERROR: userOnStart: ' + ex);
  }
}

function userOnBeforePage()
{
  try
  {
    dlog('userOnBeforePage');
    gPageStart = new Date();

    gCurrentTestId = /test=(.*);language/.exec(gSpider.mCurrentUrl.mUrl)[1];
    gCurrentTestValid = true;
    gCurrentTestStart = new Date();
  }
  catch(ex)
  {
    cdump('Spider: WARNING ERROR: userOnBeforePage: ' + ex);
    gCurrentTestValid = false;
    gPageCompleted = true;
  }
}

function userOnAfterPage()
{
  try
  {
    dlog('userOnAfterPage');
    gPageStop = new Date();

    checkTestCompleted();
  }
  catch(ex)
  {
    cdump('Spider: WARNING ERROR: userOnAfterPage: ' + ex);
    gCurrentTestValid = false;
    gPageCompleted = true;
  }
}

function userOnStop()
{
  try
  {
    // close any pending dialogs
    closeDialog();
    unregisterDialogCloser();
  }
  catch(ex)
  {
    cdump('Spider: WARNING ERROR: userOnStop: ' + ex);
  }
}

function userOnPageTimeout()
{
  gPageStop = new Date();
  if (typeof gSpider.mDocument != 'undefined')
  {
    try
    {
      var win = gSpider.mDocument.defaultView;
      if (win.wrappedJSObject)
      {
        win = win.wrappedJSObject;
      }
      gPageCompleted = win.gPageCompleted = true;
      checkTestCompleted();
    }
    catch(ex)
    {
      cdump('Spider: WARNING ERROR: userOnPageTimeout: ' + ex);
    }
  }
}

function checkTestCompleted()
{
  try
  {
    dlog('checkTestCompleted()');

    var win = gSpider.mDocument.defaultView;
    if (win.wrappedJSObject)
    {
      win = win.wrappedJSObject;
    }

    if (!gCurrentTestValid)
    {
      gPageCompleted = true;
    }
    else if (win.gPageCompleted)
    {
      gCurrentTestStop = new Date();
      // gc to flush out issues quickly
      collectGarbage();

      dlog('Page Completed');

      var gTestcases = win.gTestcases;
      if (typeof gTestcases == 'undefined')
      {
        cdump('JavaScriptTest: ' + gCurrentTestId +
              ' gTestcases array not defined. Possible test failure.');
        throw 'gTestcases array not defined. Possible test failure.';
      }
      else if (gTestcases.length == 0)
      {
        cdump('JavaScriptTest: ' + gCurrentTestId +
              ' gTestcases array is empty. Tests not run.');
        new win.TestCase(win.gTestFile, win.summary, 'Unknown', 'gTestcases array is empty. Tests not run..');
      }
      else
      {
      }
      cdump('JavaScriptTest: ' + gCurrentTestId + ' Elapsed time ' + ((gCurrentTestStop - gCurrentTestStart)/1000).toFixed(2) + ' seconds');

      gPageCompleted = true;
    }
    else
    {
      dlog('page not completed, recheck');
      setTimeout(checkTestCompleted, gCheckInterval);
    }
  }
  catch(ex)
  {
    cdump('Spider: WARNING ERROR: checkTestCompleted: ' + ex);
    gPageCompleted = true;
  } 
}

gConsoleListener.onConsoleMessage =
  function userOnConsoleMessage(s)
{
  dump(s);
};
