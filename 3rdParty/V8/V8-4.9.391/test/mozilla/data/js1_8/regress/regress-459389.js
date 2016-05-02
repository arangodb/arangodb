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

var gTestfile = 'regress-459389.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 459389;
var summary = 'Do not crash with JIT';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
print('mmmm, food!');

jit(true);

var SNI = {};
SNI.MetaData={};
SNI.MetaData.Parameter=function()
{
var parameters={}; 
this.addParameter=function(key,value)
{
parameters[key]=[];
parameters[key].push(value);
};
this.getParameter=function(key,separator){
if(!parameters[key])
{
return;
} 
return parameters[key].join(separator);
};
this.getKeys=function()
{
return parameters;
};
};
SNI.MetaData.Manager=function(){
var m=new SNI.MetaData.Parameter(); 
this.getParameter=m.getParameter; 
};
var MetaDataManager=SNI.MetaData.Manager;
SNI.Ads={ };
SNI.Ads.Url=function(){
var p=new SNI.MetaData.Parameter();
this.addParameter=p.addParameter;
this.getParameter=p.getParameter;
};
function Ad() {
var url=new SNI.Ads.Url();
this.addParameter=url.addParameter;
this.getParameter=url.getParameter;
}
function DartAd()
AdUrl.prototype=new Ad();
function AdUrl() { }
function AdRestriction() {
var p=new SNI.MetaData.Parameter();
this.addParameter=p.addParameter;
this.getParameter=p.getParameter;
this.getKeys=p.getKeys;
}
function AdRestrictionManager(){
this.restriction=[];
this.isActive=isActive;
this.isMatch=isMatch;
this.startMatch=startMatch;
function isActive(ad,mdm){
var value=false;
for(var i=0;i<this.restriction.length;i++)
{
adRestriction=this.restriction[i];
value=this.startMatch(ad,mdm,adRestriction);
} 
}
function startMatch(ad,mdm,adRestriction){
for(var key in adRestriction.getKeys()){
var restrictions=adRestriction.getParameter(key,',');
var value=mdm.getParameter(key,'');
match=this.isMatch(value,restrictions);
if(!match){
value=ad.getParameter(key,'')
match=this.isMatch(value,restrictions);
} 
} 
}
function isMatch(value,restrictions){
var match=false;
if(value)
{
splitValue=value.split('');
for(var x=0;x<splitValue.length;x++){
for(var a;a<restrictions.length;a++){ }
}
} 
return match;
}
}
var adRestrictionManager = new AdRestrictionManager();
var restrictionLeader6 = new AdRestriction();
restrictionLeader6.addParameter("", "");
adRestrictionManager.restriction.push(restrictionLeader6);
var restrictionLeader7 = new AdRestriction();
restrictionLeader7.addParameter("", "");
adRestrictionManager.restriction.push(restrictionLeader7);
function FoodAd(adtype)
{
  ad=new DartAd()
  ad.addParameter("",adtype)
  adRestrictionManager.isActive(ad,	mdManager)
}
var mdManager = new MetaDataManager() ;
  FoodAd('P')

jit(false);

reportCompare(expect, actual, summary);
