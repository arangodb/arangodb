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
 * Jeff Walden <jwalden+code@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2006
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

gTestfile = 'regress-350629.js';

//-----------------------------------------------------------------------------
var BUGNUMBER     = "350629";
var summary = ".toXMLString can include invalid generated prefixes";
var actual, expect;

printBugNumber(BUGNUMBER);
START(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;

function extractPrefix(el, attrName, attrVal)
{
  var str = el.toXMLString();
  var regex = new RegExp(' (.+?):' + attrName + '="' + attrVal + '"');
  return str.match(regex)[1];
}

function assertValidPrefix(p, msg)
{
  if (!isXMLName(p) ||
      0 == p.search(/xml/i))
    throw msg;
}

var el, n, p;

try
{
  // last component is invalid prefix
  el = <foo/>;
  n = new Namespace("http://foo/bar.xml");
  el.@n::fiz = "eit";
  p = extractPrefix(el, "fiz", "eit");
  assertValidPrefix(p, "namespace " + n.uri + " generated invalid prefix " + p);

  // last component is invalid prefix (different case)
  el = <foo/>;
  n = new Namespace("http://foo/bar.XML");
  el.@n::fiz = "eit";
  p = extractPrefix(el, "fiz", "eit");
  assertValidPrefix(p, "namespace " + n.uri + " generated invalid prefix " + p);

  // last component is invalid prefix (but not "xml"/"xmlns")
  el = <foo/>;
  n = new Namespace("http://foo/bar.xmln");
  el.@n::baz = "quux";
  p = extractPrefix(el, "baz", "quux");
  assertValidPrefix(p, "namespace " + n.uri + " generated invalid prefix " + p);


  // generated prefix with no valid prefix component in namespace URI
  el = <foo/>;
  n = new Namespace("xml:///");
  el.@n::bike = "cycle";
  p = extractPrefix(el, "bike", "cycle");
  assertValidPrefix(p, "namespace " + n.uri + " generated invalid prefix " + p);


  // generated prefix with no valid prefix component in namespace URI w/further
  // collision
  el = <aaaaa:foo xmlns:aaaaa="http://baz/"/>;
  n = new Namespace("xml:///");
  el.@n::bike = "cycle";
  p = extractPrefix(el, "bike", "cycle");
  assertValidPrefix(p, "namespace " + n.uri + " generated invalid prefix " + p);



  // XXX this almost certainly shouldn't work, so if it fails at some time it
  //     might not be a bug!  it's only here because it *is* currently a
  //     possible failure point for prefix generation
  el = <foo/>;
  n = new Namespace(".:/.././.:/:");
  el.@n::biz = "17";
  p = extractPrefix(el, "biz", "17");
  assertValidPrefix(p, "namespace " + n.uri + " generated invalid prefix " + p);
}
catch (ex)
{
  failed = ex;
}

expect = false;
actual = failed;

TEST(1, expect, actual);
