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
 * Contributor(s): Rob Sayre
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

var gTestfile = 'regress-449627.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 449627;
var summary = 'Crash with JIT in js_FillPropertyCache';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

jit(true);

/************************ BROWSER DETECT (http://www.quirksmode.org/js/detect.html) ************************/

if (typeof navigator == 'undefined')
{
  navigator = {
    userAgent: "Firefox",
    vendor: "Mozilla",
    platform: "Mac"
  };
}

global = this;

var BrowserDetect = {
    init: function _init()
    {
      this.browser=this.searchString(this.dataBrowser) || "An unknown browser";

      this.OS= this.searchString(this.dataOS)||"an unknown OS";
    },
    searchString: function _searchString(a)
    {
      for(var i=0; i < a.length; i++)
      {
	var b=a[i].string;
	var c=a[i].prop;
	this.versionSearchString=a[i].versionSearch||a[i].identity;
	if(b)
	{
	  if(b.indexOf(a[i].subString)!=-1)
	    return a[i].identity;
        }
	else if(c)
	return a[i].identity;
      }
    },

    searchVersion:function _searchVersion(a)
    {
      var b=a.indexOf(this.versionSearchString);
      if(b==-1)
      	return;
      return parseFloat(a.substring(b+this.versionSearchString.length+1));
    },

    dataBrowser:[
      {
	string:navigator.userAgent,subString:"OmniWeb",versionSearch:"OmniWeb/",identity:"OmniWeb"
      },
      {
	string:navigator.vendor,subString:"Apple",identity:"Safari"
      },
      {
	prop:global.opera,identity:"Opera"
      },
      {
	string:navigator.vendor,subString:"iCab",identity:"iCab"
      },
      {
	string:navigator.vendor,subString:"KDE",identity:"Konqueror"
      },
      {
	string:navigator.userAgent,subString:"Firefox",identity:"Firefox"
      },
      {
	string:navigator.vendor,subString:"Camino",identity:"Camino"
      },
      {
	string:navigator.userAgent,subString:"Netscape",identity:"Netscape"
      },
      {
	string:navigator.userAgent,subString:"MSIE",identity:"Explorer",versionSearch:"MSIE"
      },
      {
	string:navigator.userAgent,subString:"Gecko",identity:"Mozilla",versionSearch:"rv"
      },
      {
	string:navigator.userAgent,subString:"Mozilla",identity:"Netscape",versionSearch:"Mozilla"
      }
    ],
    dataOS:[
      {
	string:navigator.platform,subString:"Win",identity:"Windows"
      },
      {
	string:navigator.platform,subString:"Mac",identity:"Mac"
      },
      {
	string:navigator.platform,subString:"Linux",identity:"Linux"
      }
    ]
  };

BrowserDetect.init();

jit(false);

reportCompare(expect, actual, summary);
