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
 * The Original Code is mozilla.org code.
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

var gPageCompleted;
var GLOBAL = this + '';

function htmlesc(str) {
  if (str == '<')
    return '&lt;';
  if (str == '>')
    return '&gt;';
  if (str == '&')
    return '&amp;';
  return str;
}

function DocumentWrite(s)
{
  try
  {
    var msgDiv = document.createElement('div');
    msgDiv.innerHTML = s;
    document.body.appendChild(msgDiv);
    msgDiv = null;
  }
  catch(excp)
  {
    document.write(s + '<br>\n');
  }
}

function print() {
  var s = '';
  var a;
  for (var i = 0; i < arguments.length; i++)
  {
    a = arguments[i];
    s += String(a) + ' ';
  }

  if (typeof dump == 'function')
  {
    dump( s + '\n');
  }

  s = s.replace(/[<>&]/g, htmlesc);

  DocumentWrite(s);
}

function writeHeaderToLog( string ) {
  string = String(string);

  if (typeof dump == 'function')
  {
    dump( string + '\n');
  }

  string = string.replace(/[<>&]/g, htmlesc);

  DocumentWrite( "<h2>" + string + "</h2>" );
}

function writeFormattedResult( expect, actual, string, passed ) {
  string = String(string);

  if (typeof dump == 'function')
  {
    dump( string + '\n');
  }

  string = string.replace(/[<>&]/g, htmlesc);

  var s = "<tt>"+ string ;
  s += "<b>" ;
  s += ( passed ) ? "<font color=#009900> &nbsp;" + PASSED
    : "<font color=#aa0000>&nbsp;" +  FAILED + expect + "</tt>";

  DocumentWrite( s + "</font></b></tt><br>" );
  return passed;
}

window.onerror = function (msg, page, line)
{
  optionsPush();

  if (typeof DESCRIPTION == 'undefined')
  {
    DESCRIPTION = 'Unknown';
  }
  if (typeof EXPECTED == 'undefined')
  {
    EXPECTED = 'Unknown';
  }

  var testcase = new TestCase(gTestfile, DESCRIPTION, EXPECTED, "error");

  if (document.location.href.indexOf('-n.js') != -1)
  {
    // negative test
    testcase.passed = true;
  }

  testcase.reason = page + ':' + line + ': ' + msg;

  reportFailure(msg);

  optionsReset();
};

function gc()
{
  // Thanks to igor.bukanov@gmail.com
  for (var i = 0; i != 4e6; ++i)
  {
    var tmp = i + 0.1;
  }
}

function jsdgc()
{
  try
  {
    // Thanks to dveditz
    netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
    var jsdIDebuggerService = Components.interfaces.jsdIDebuggerService;
    var service = Components.classes['@mozilla.org/js/jsd/debugger-service;1'].
      getService(jsdIDebuggerService);
    service.GC();
  }
  catch(ex)
  {
    print('gc: ' + ex);
  }
}

function quit()
{
}

function Preferences(aPrefRoot)
{
  try
  {
    this.orig = {};
    this.privs = 'UniversalXPConnect UniversalPreferencesRead ' +
      'UniversalPreferencesWrite';

    if (typeof netscape != 'undefined' &&
        'security' in netscape &&
        'PrivilegeManager' in netscape.security &&
        'enablePrivilege' in netscape.security.PrivilegeManager)
    {
      netscape.security.PrivilegeManager.enablePrivilege(this.privs);

      var nsIPrefService = Components.interfaces.nsIPrefService;
      var nsIPrefBranch = Components.interfaces.nsIPrefBranch;
      var nsPrefService_CONTRACTID = "@mozilla.org/preferences-service;1";

      this.prefRoot    = aPrefRoot;
      this.prefService = Components.classes[nsPrefService_CONTRACTID].
        getService(nsIPrefService);
      this.prefBranch = this.prefService.getBranch(aPrefRoot).
        QueryInterface(Components.interfaces.nsIPrefBranch2);
    }
  }
  catch(ex)
  {
  }

}

function Preferences_getPrefRoot()
{
  var root;

  try
  {
    if (typeof netscape != 'undefined' &&
        'security' in netscape &&
        'PrivilegeManager' in netscape.security &&
        'enablePrivilege' in netscape.security.PrivilegeManager)
    {
      netscape.security.PrivilegeManager.enablePrivilege(this.privs);
    }

    root = this.prefBranch.root;
  }
  catch(ex)
  {
  }
  return root;
}

function Preferences_getPref(aPrefName)
{
  var value;
  try
  {
    if (typeof netscape != 'undefined' &&
        'security' in netscape &&
        'PrivilegeManager' in netscape.security &&
        'enablePrivilege' in netscape.security.PrivilegeManager)
    {
      netscape.security.PrivilegeManager.enablePrivilege(this.privs);
      value = this.prefBranch.getBoolPref(aPrefName);
    }
  }
  catch(ex)
  {
  }
  return value;
}

function Preferences_getBoolPref(aPrefName)
{
  var value;
  try
  {
    if (typeof netscape != 'undefined' &&
        'security' in netscape &&
        'PrivilegeManager' in netscape.security &&
        'enablePrivilege' in netscape.security.PrivilegeManager)
    {
      netscape.security.PrivilegeManager.enablePrivilege(this.privs);
      value = this.prefBranch.getBoolPref(aPrefName);
    }
  }
  catch(ex)
  {
  }
  return value;
}

function Preferences_getIntPref(aPrefName)
{
  var value;
  try
  {
    if (typeof netscape != 'undefined' &&
        'security' in netscape &&
        'PrivilegeManager' in netscape.security &&
        'enablePrivilege' in netscape.security.PrivilegeManager)
    {
      netscape.security.PrivilegeManager.enablePrivilege(this.privs);
      value = this.prefBranch.getIntPref(aPrefName);
    }
  }
  catch(ex)
  {
  }
  return value;
}

function Preferences_getCharPref(aPrefName)
{
  var value;
  try
  {
    if (typeof netscape != 'undefined' &&
        'security' in netscape &&
        'PrivilegeManager' in netscape.security &&
        'enablePrivilege' in netscape.security.PrivilegeManager)
    {
      netscape.security.PrivilegeManager.enablePrivilege(this.privs);
      value = this.prefBranch.getCharPref(aPrefName);
    }
  }
  catch(ex)
  {
  }
  return value;
}

function Preferences_setPref(aPrefName, aPrefValue)
{
  var value;

  try
  {
    if (typeof netscape != 'undefined' &&
        'security' in netscape &&
        'PrivilegeManager' in netscape.security &&
        'enablePrivilege' in netscape.security.PrivilegeManager)
    {
      netscape.security.PrivilegeManager.enablePrivilege(this.privs);

      if (typeof this.orig[aPrefName] == 'undefined')
      {
        this.orig[aPrefName] = this.getPref(aPrefName);
      }

      value = this.prefBranch.setBoolPref(aPrefName, Boolean(aPrefValue));
    }
  }
  catch(ex)
  {
  }
}

function Preferences_setBoolPref(aPrefName, aPrefValue)
{
  var value;

  try
  {
    if (typeof netscape != 'undefined' &&
        'security' in netscape &&
        'PrivilegeManager' in netscape.security &&
        'enablePrivilege' in netscape.security.PrivilegeManager)
    {
      netscape.security.PrivilegeManager.enablePrivilege(this.privs);

      if (typeof this.orig[aPrefName] == 'undefined')
      {
        this.orig[aPrefName] = this.getBoolPref(aPrefName);
      }

      value = this.prefBranch.setBoolPref(aPrefName, Boolean(aPrefValue));
    }
  }
  catch(ex)
  {
  }
}

function Preferences_setIntPref(aPrefName, aPrefValue)
{
  var value;

  try
  {
    if (typeof netscape != 'undefined' &&
        'security' in netscape &&
        'PrivilegeManager' in netscape.security &&
        'enablePrivilege' in netscape.security.PrivilegeManager)
    {
      netscape.security.PrivilegeManager.enablePrivilege(this.privs);

      if (typeof this.orig[aPrefName] == 'undefined')
      {
        this.orig[aPrefName] = this.getIntPref(aPrefName);
      }

      value = this.prefBranch.setIntPref(aPrefName, Number(aPrefValue));
    }
  }
  catch(ex)
  {
  }
}

function Preferences_setCharPref(aPrefName, aPrefValue)
{
  var value;

  try
  {
    if (typeof netscape != 'undefined' &&
        'security' in netscape &&
        'PrivilegeManager' in netscape.security &&
        'enablePrivilege' in netscape.security.PrivilegeManager)
    {
      netscape.security.PrivilegeManager.enablePrivilege(this.privs);

      if (typeof this.orig[aPrefName] == 'undefined')
      {
        this.orig[aPrefName] = this.getCharPref(aPrefName);
      }

      value = this.prefBranch.setCharPref(aPrefName, String(aPrefValue));
    }
  }
  catch(ex)
  {
  }
}

function Preferences_resetPref(aPrefName)
{
  try
  {
    if (typeof netscape != 'undefined' &&
        'security' in netscape &&
        'PrivilegeManager' in netscape.security &&
        'enablePrivilege' in netscape.security.PrivilegeManager)
    {
      netscape.security.PrivilegeManager.enablePrivilege(this.privs);

      if (aPrefName in this.orig)
      {
        if (typeof this.orig[aPrefName] == 'undefined')
        {
          this.clearPref(aPrefName);
        }
        else
        {
          this.setPref(aPrefName, this.orig[aPrefName]);
        }
      }
    }
  }
  catch(ex)
  {
  }
}

function Preferences_resetAllPrefs()
{
  try
  {
    var prefName;
    var prefValue;

    if (typeof netscape != 'undefined' &&
        'security' in netscape &&
        'PrivilegeManager' in netscape.security &&
        'enablePrivilege' in netscape.security.PrivilegeManager)
    {
      netscape.security.PrivilegeManager.enablePrivilege(this.privs);
      for (prefName in this.orig)
      {
        this.setPref(prefName, this.orig[prefName]);
      }
    }
  }
  catch(ex)
  {
  }
}

function Preferences_clearPref(aPrefName)
{
  try
  {
    if (typeof netscape != 'undefined' &&
        'security' in netscape &&
        'PrivilegeManager' in netscape.security &&
        'enablePrivilege' in netscape.security.PrivilegeManager)
    {
      netscape.security.PrivilegeManager.enablePrivilege(this.privs);
      this.prefBranch.clearUserPref(aPrefName);
    }
  }
  catch(ex)
  {
  }
}

Preferences.prototype.getPrefRoot    = Preferences_getPrefRoot;
Preferences.prototype.getPref        = Preferences_getPref;
Preferences.prototype.getBoolPref    = Preferences_getBoolPref;
Preferences.prototype.getIntPref     = Preferences_getIntPref;
Preferences.prototype.getCharPref    = Preferences_getCharPref;
Preferences.prototype.setPref        = Preferences_setPref;
Preferences.prototype.setBoolPref    = Preferences_setBoolPref;
Preferences.prototype.setIntPref     = Preferences_setIntPref;
Preferences.prototype.setCharPref    = Preferences_setCharPref;
Preferences.prototype.resetAllPrefs  = Preferences_resetAllPrefs;
Preferences.prototype.resetPref      = Preferences_resetPref;
Preferences.prototype.clearPref      = Preferences_clearPref;

function options(aOptionName)
{
  // return value of options() is a comma delimited list
  // of the previously set values

  var value = '';
  for (var optionName in options.currvalues)
  {
    value += optionName + ',';
  }
  if (value)
  {
    value = value.substring(0, value.length-1);
  }

  if (aOptionName)
  {
    if (options.currvalues[aOptionName])
    {
      // option is set, toggle it to unset
      delete options.currvalues[aOptionName];
      options.preferences.setPref(aOptionName, false);
    }
    else
    {
      // option is not set, toggle it to set
      options.currvalues[aOptionName] = true;
      options.preferences.setPref(aOptionName, true);
    }
  }

  return value;
}

function optionsInit() {

  // hash containing the set options
  options.currvalues = {strict:     '',
                        werror:     '',
                        atline:     '',
                        xml:        '',
                        relimit:    '',
                        anonfunfux: ''
  }

  // record initial values to support resetting
  // options to their initial values
  options.initvalues  = {};

  // record values in a stack to support pushing
  // and popping options
  options.stackvalues = [];

  options.preferences = new Preferences('javascript.options.');

  for (var optionName in options.currvalues)
  {
    if (!options.preferences.getPref(optionName))
    {
      delete options.currvalues[optionName];
    }
    else
    {
      options.initvalues[optionName] = '';
    }
  }
}

function gczeal(z)
{
  var javascriptoptions = new Preferences('javascript.options.');
  javascriptoptions.setIntPref('gczeal', Number(z));
}

var gJit = { content: undefined, chrome: undefined };

function jit(on)
{
  var jitoptions = new Preferences('javascript.options.jit.');

  if (typeof gJit.content == 'undefined')
  {
    gJit.content = jitoptions.getBoolPref('content');
    gJit.chrome  = jitoptions.getBoolPref('chrome');
  }

  if (on)
  {
    jitoptions.setBoolPref('content', true);
    jitoptions.setBoolPref('chrome', false);
  }
  else
  {
    jitoptions.setBoolPref('content', false);
    jitoptions.setBoolPref('chrome', false);
  }
}

var gVersion = 150;

function jsTestDriverBrowserInit()
{
  if (typeof dump != 'function')
  {
    dump = print;
  }

  optionsInit();
  optionsClear();

  if (document.location.search.indexOf('?') != 0)
  {
    // not called with a query string
    return;
  }

  var properties = {};
  var fields = document.location.search.slice(1).split(';');
  for (var ifield = 0; ifield < fields.length; ifield++)
  {
    var propertycaptures = /^([^=]+)=(.*)$/.exec(fields[ifield]);
    if (!propertycaptures)
    {
      properties[fields[ifield]] = true;
    }
    else
    {
      properties[propertycaptures[1]] = decodeURIComponent(propertycaptures[2]);
      if (propertycaptures[1] == 'language')
      {
        // language=(type|language);mimetype
        properties.mimetype = fields[ifield+1];
      }
    }
  }

  if (properties.language != 'type')
  {
    try
    {
      properties.version = /javascript([.0-9]+)/.exec(properties.mimetype)[1];
    }
    catch(ex)
    {
    }
  }

  if (!properties.version && navigator.userAgent.indexOf('Gecko/') != -1)
  {
    // If the version is not specified, and the browser is Gecko,
    // adjust the version to match the suite version.
    if (properties.test.match(/^js1_6/))
    {
      properties.version = '1.6';
    }
    else if (properties.test.match(/^js1_7/))
    {
      properties.version = '1.7';
    }
    else if (properties.test.match(/^js1_8/))
    {
      properties.version = '1.8';
    }
    else if (properties.test.match(/^js1_8_1/))
    {
      properties.version = '1.8';
    }
    else
    {
      properties.version = '1.5';
    }
  }

  gTestPath = properties.test;

  gVersion = 10*parseInt(properties.version.replace(/\./g, ''));

  if (properties.gczeal)
  {
    gczeal(Number(properties.gczeal));
  }

  /*
   * since the default setting of jit changed from false to true
   * in http://hg.mozilla.org/tracemonkey/rev/685e00e68be9
   * bisections which depend upon jit settings can be thrown off.
   * default jit(false) to make bisections depending upon jit settings
   * consistent over time. This is not needed in shell tests as the default
   * jit setting has not changed there.
   */

  jit(properties.jit);

  var testpathparts = properties.test.split(/\//);

  if (testpathparts.length < 3)
  {
    // must have at least suitepath/subsuite/testcase.js
    return;
  }
  var suitepath = testpathparts.slice(0,testpathparts.length-2).join('/');
  var subsuite = testpathparts[testpathparts.length - 2];
  var test     = testpathparts[testpathparts.length - 1];

  document.write('<title>' + suitepath + '/' + subsuite + '/' + test + '<\/title>');

  // XXX bc - the first document.written script is ignored if the protocol
  // is file:. insert an empty script tag, to work around it.
  document.write('<script></script>');

  outputscripttag(suitepath + '/shell.js', properties);
  outputscripttag(suitepath + '/browser.js', properties);
  outputscripttag(suitepath + '/' + subsuite + '/shell.js', properties);
  outputscripttag(suitepath + '/' + subsuite + '/browser.js', properties);
  outputscripttag(suitepath + '/' + subsuite + '/' + test, properties,
  	properties.e4x || /e4x\//.test(properties.test));
  outputscripttag('js-test-driver-end.js', properties);
  return;
}

function outputscripttag(src, properties, e4x)
{
  if (!src)
  {
    return;
  }

  if (e4x)
  {
    // e4x requires type=mimetype;e4x=1
    properties.language = 'type';
  }

  var s = '<script src="' +  src + '" ';

  if (properties.language != 'type')
  {
    s += 'language="javascript';
    if (properties.version)
    {
      s += properties.version;
    }
  }
  else
  {
    s += 'type="' + properties.mimetype;
    if (properties.version)
    {
      s += ';version=' + properties.version;
    }
    if (e4x)
    {
      s += ';e4x=1';
    }
  }
  s += '"><\/script>';

  document.write(s);
}

function jsTestDriverEnd()
{
  // gDelayTestDriverEnd is used to
  // delay collection of the test result and
  // signal to Spider so that tests can continue
  // to run after page load has fired. They are
  // responsible for setting gDelayTestDriverEnd = true
  // then when completed, setting gDelayTestDriverEnd = false
  // then calling jsTestDriverEnd()

  if (gDelayTestDriverEnd)
  {
    return;
  }

  window.onerror = null;

  try
  {
    var javascriptoptions = new Preferences('javascript.options.');
    javascriptoptions.clearPref('gczeal');

    var jitoptions = new Preferences('javascript.options.jit.');
    if (typeof gJit.content != 'undefined')
    {
      jitoptions.setBoolPref('content', gJit.content);
    }

    if (typeof gJit.chrome != 'undefined')
    {
      jitoptions.setBoolPref('chrome', gJit.chrome);
    }

    optionsReset();
  }
  catch(ex)
  {
    dump('jsTestDriverEnd ' + ex);
  }

  if (window.opener && window.opener.runNextTest)
  {
    if (window.opener.reportCallBack)
    {
      window.opener.reportCallBack(window.opener.gWindow);
    }
    setTimeout('window.opener.runNextTest()', 250);
  }
  else
  {
    for (var i = 0; i < gTestcases.length; i++)
    {
      gTestcases[i].dump();
    }

    // tell Spider page is complete
    gPageCompleted = true;
  }
}

jsTestDriverBrowserInit();
