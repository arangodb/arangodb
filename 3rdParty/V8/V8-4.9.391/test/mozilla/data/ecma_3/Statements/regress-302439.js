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
 * Contributor(s): Silviu Trasca
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

var gTestfile = 'regress-302439.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 302439;
var summary = 'spandep fu should skip unused JSOP_TABLESWITCH jump table entries';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

function productList(catID,famID) {
  clearBox(document.Support_Form.Product_ID);

  switch(parseInt(catID)) {

  case 1 :                             // Sound Blaster
    switch(parseInt(famID)) {

    case 434 :                     // Audigy 4
      break;

    case 204 :                     // Audigy 2
      break;

    case 205 :                     // Audigy

      try { addBoxItem(document.Support_Form.Product_ID, 'Audigy Platinum eX', '45'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Audigy Platinum', '4846'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Audigy LS (SE)', '10365'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Audigy Digital Entertainment', '5085'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Audigy ES', '5086'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 206 :                     // Live!
      try { addBoxItem(document.Support_Form.Product_ID, 'Live! 24-bit External', '10702'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Sound Blaster Live! MP3+ 5.1', '573'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Live! 5.1', '50'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Live! Digital Entertainment 5.1 (SE)', '4855'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Live! Platinum 5.1', '572'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Sound Blaster Live! 5.1 Digital (Dell)', '1853'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Live! Platinum', '3203'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Sound Blaster Live! Value', '4856'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Sound Blaster Live!', '4857'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 207 :                     // Others
      try { addBoxItem(document.Support_Form.Product_ID, 'Extigy', '585'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Ensoniq AudioPCI', '420'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'PCI4.1 Digital', '681'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Vibra128 4D', '9032'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Digital Music', '154'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Vibra 128', '4851'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Sound Blaster 32', '1844'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'SB AWE64 Digital', '1821'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'SB PCI 5.1', '1828'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Sound Blaster\u00AE', '1841'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Sound Blaster\u00AE 16', '1842'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Sound Blaster 16 Wave Effects', '1843'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Sound Blaster AWE32', '1848'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Sound Blaster AWE64', '1849'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Sound Blaster AWE64 Gold', '1850'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Sound Blaster Microchannels', '1861'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Sound Blaster PCI 128', '1864'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Sound Blaster PCI 64', '1865'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Sound Blaster Pro', '1866'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Sound Blaster Audio PCI', '1559'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 16 :                     // Accessories
      try { addBoxItem(document.Support_Form.Product_ID, 'Live!Drive II', '9278'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Sound Blaster MIDI Adapter', '251'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Mini to Standard MIDI Adapter', '252'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;


    }
    break;

  case 210 :                             // Portable Media Players
    switch(parseInt(famID)) {

    case 211 :                     // Zen
      try { addBoxItem(document.Support_Form.Product_ID, 'Zen Portable Media Center', '9882'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 212 :                     // Accessories
      try { addBoxItem(document.Support_Form.Product_ID, 'Zen PMC Docking Station', '10756'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Zen PMC Li-ion Polymer Battery', '10679'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Zen PMC FM Wired Remote', '10663'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;


    }
    break;

  case 213 :                             // MP3 Players
    switch(parseInt(famID)) {

    case 214 :                     // Zen
      try { addBoxItem(document.Support_Form.Product_ID, 'Zen Touch', '10274'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Zen Micro', '10795'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Zen', '11519'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Zen Xtra', '9288'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Zen NX', '4836'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Zen USB 2.0', '9019'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative NOMAD Jukebox Zen', '117'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 215 :                     // MuVo
      try { addBoxItem(document.Support_Form.Product_ID, 'MuVo Micro N200', '10737'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'MuVo\u00B2 X-Trainer', '5080'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'MuVo Slim', '10052'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'MuVo Sport C100', '10794'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'MuVo V200', '10732'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'MuVo TX FM', '9771'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'MuVo USB 2.0', '10919'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'MuVo', '110'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'MuVo NX', '4884'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'MuVo\u00B2', '4908'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'MuVo TX', '9672'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 216 :                     // Digital MP3 Player
      try { addBoxItem(document.Support_Form.Product_ID, 'Zen Xtra', '9288'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Rhomba NX', '10302'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'MP3 Player FX120', '11010'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'DXT 200', '4996'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Zen USB 2.0', '9019'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Jukebox 3', '296'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative CD-MP3 Slim 600', '1582'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Zen NX', '4836'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'MP3 Player', '4983'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'MP3 Player 2', '4984'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'MP3 Player MX', '4985'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative NOMAD Jukebox Zen', '117'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'JukeBox 2', '239'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative NOMAD JukeBox', '241'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative NOMAD JukeBox C', '242'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Jukebox 10GB', '261'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative CD-MP3 M100', '264'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 217 :                     // Accessories
      try { addBoxItem(document.Support_Form.Product_ID, 'Zen Micro Battery', '11215'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Universal Travel Adapter for Zen Micro', '11711'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Zen Neeon Stik-On', '12982'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Zen Neeon Universal Travel Adapter', '12979'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Leather Case', '11511'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Zen Neeon Leather Case', '12978'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Home Kit - Jukebox 3', '497'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative NOMAD Jukebox 3 Leather Case', '498'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Faceplates - Jukebox 3', '499'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'MuVo Armband', '511'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'NOMAD II MG Wired Remote', '515'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative NOMAD Jukebox Accessory Kit', '533'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative NOMAD Jukebox Battery Charger Kit', '538'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative NOMAD Jukebox Battery Pack', '539'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'NOMAD II MG Travel Cable', '560'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Leather Case - Jukebox 2', '562'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Battery - Jukebox 2', '563'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'MuVo Battery Modules', '999'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'PlayDock PD200', '31'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'TravelSound', '80'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Li-Ion Battery - Jukebox 3', '86'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'FM Wired Remote - Jukebox 3/Jukebox Zen/MuVo\u00B2', '115'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative NOMAD Jukebox Power Adapter', '125'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Cassette Adapter Kit', '401'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Car Kit - Jukebox 3/Jukebox 2/Jukebox Zen/MuVo\u00B2', '496'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Battery Pack', '4997'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Battery Modules - MuVo NX / TX / TX FM', '9217'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Armband - MuVo NX / TX / TX FM', '10126'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;


    }
    break;

  case 4 :                             // Speaker Systems
    switch(parseInt(famID)) {

    case 113 :                     // 7.1 Systems
      try { addBoxItem(document.Support_Form.Product_ID, 'Inspire T7700', '5076'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 24 :                     // 6.1 Systems
      try { addBoxItem(document.Support_Form.Product_ID, 'Inspire 6.1 6600', '465'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 25 :                     // 5.1 Systems
      try { addBoxItem(document.Support_Form.Product_ID, 'Creative Inspire 5.1 5100', '1704'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'PCWorks LX520', '9412'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'I-Trigue 5600', '10736'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Inspire T5900', '10323'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Inspire P5800', '10596'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Desktop Theater 5.1 DTT2200', '428'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Inspire 5.1 5700 Digital', '439'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative Inspire 5.1 Digital 5500', '990'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Inspire 5.1 5200', '55'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative Inspire 5.1 5300', '238'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'MegaWorks THX 5.1 550', '240'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Desktop Theater 5.1 DTT3500 Digital', '290'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'PlayWorks DTT2500 Digital', '291'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative Inspire 5.1 5600', '1705'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Inspire T5400', '5077'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'PlayWorks PS2000 Digital', '427'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Desktop Theater 5.1', '1628'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Desktop Theater 5.1 DTT2500 Digital', '1629'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Cambridge SoundWorks MegaWorks 510D', '478'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 26 :                     // 4.1 Systems
      try { addBoxItem(document.Support_Form.Product_ID, 'FPS1600', '47'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'FPS2000 Digital', '297'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Inspire 4.1 4400', '446'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'FPS1800', '424'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'PC-Works FourPointSurround FPS1000', '378'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'FPS1500', '388'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 27 :                     // 2.1 Systems
      try { addBoxItem(document.Support_Form.Product_ID, 'I-Trigue 3600', '10735'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Inspire T3000', '10329'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'I-Trigue 3400', '10733'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Inspire G380', '9276'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative I-Trigue 3200', '10327'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'PCWorks LX220', '9407'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative Inspire 2.1 Slim 2600', '434'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Inspire 2.1 2500', '461'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'I-Trigue L3500', '4912'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'I-Trigue L3450', '4913'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Inspire T2900', '9025'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Inspire P380', '9026'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'SoundWorks SW320', '48'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'MegaWorks THX 2.1 250D', '124'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'I-Trigue 2.1 3300', '139'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative SBS 2.1 350 Speakers', '281'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'SBS 370', '283'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'PCWorks', '284'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative Inspire Slim 500', '289'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative Inspire 2.1 2400', '298'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'SoundWorks Digital', '299'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'SoundWorks SW310', '304'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'I-Trigue i3350', '279'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 28 :                     // 2.0 Systems
      try { addBoxItem(document.Support_Form.Product_ID, 'Inspire Monitor M85-D', '4910'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Inspire 2.0 1300', '4918'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'SBS 230 Speakers', '4905'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'SBS52', '1'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'SBS16', '2'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Cambridge SBS20', '3'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'SBS50', '1834'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'SBS10', '1831'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative SBS15', '4906'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 29 :                     // Portable Systems
      try { addBoxItem(document.Support_Form.Product_ID, 'TravelSound 200', '10164'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'TravelSound MP3', '1874'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'PlayDock PD200', '31'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'TravelSound', '80'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'TravelSound i-300', '9022'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative TravelSound MP3 Titanium', '991'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 136 :                     // Decoders
      try { addBoxItem(document.Support_Form.Product_ID, 'Decoder DDTS-100', '9468'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 30 :                     // Accessories
      try { addBoxItem(document.Support_Form.Product_ID, 'MT-1100 Speaker Stands', '166'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Headphones HQ-1700', '11164'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Headphones HQ-1300', '4936'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Headphones HN-505', '4938'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Backphones HQ-65', '4916'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Backphones HQ-60', '4937'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Earphones EP-880', '11156'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Earphones EP-480', '11708'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Headset HE-100', '11023'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Headset HS-300', '4939'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'MT-1200 Speaker Stands', '9515'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'SurroundStation', '32'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative HQ-2000 Headphones', '4'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'MT-500 Speaker Tripods', '399'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Inspire 2600 Spkr Grilles', '636'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Inspire 5300 Spkr Grilles', '637'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Inspire 5700 Spkr Grilles', '664'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative HQ-1000 Headphones', '4988'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;


    }
    break;

  case 218 :                             // Web Cameras
    switch(parseInt(famID)) {

    case 219 :                     // WebCam
      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam Live! Ultra for Notebooks', '11491'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam Live! Ultra', '10451'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam Live! Pro', '10450'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam Live!', '10412'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam Instant', '10410'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam NX Ultra', '9340'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam NX Pro', '628'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam NX', '627'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam PRO eX', '243'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam PRO', '616'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam Go Plus', '15'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Webcam Go ES', '1898'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam Go Mini', '1900'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam Go', '17'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Video Blaster WebCam Plus', '16'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam Notebook', '629'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam Mobile', '4890'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam 5', '1896'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam Vista Pro', '11053'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam Vista Plus', '11043'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam', '65'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam II', '4900'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam 3', '1908'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam Vista', '1907'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 220 :                     // PC-CAM
      try { addBoxItem(document.Support_Form.Product_ID, 'PC-CAM 900', '10119'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'PC-CAM 930 Slim', '11431'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'PC-CAM 920 Slim', '10823'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'PC-CAM 880', '308'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'PC-CAM 750', '4878'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'PC-CAM 850', '4879'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative PC-CAM 300', '49'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'PC-CAM 350', '106'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'PC-CAM 550', '107'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'CardCam Value', '116'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'PC-CAM 600', '260'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;


    }
    break;

  case 437 :                             // Headphones &amp; Headsets
    switch(parseInt(famID)) {

    case 438 :                     // Headphones
      try { addBoxItem(document.Support_Form.Product_ID, 'Headphones HQ-1700', '11164'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Headphones HQ-1300', '4936'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Headphones CB2530', '11644'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 439 :                     // Noise-Cancelling Headphones
      try { addBoxItem(document.Support_Form.Product_ID, 'Headphones HN-505', '4938'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 440 :                     // Backphones
      try { addBoxItem(document.Support_Form.Product_ID, 'Backphones HQ-65', '4916'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Backphones HQ-60', '4937'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 441 :                     // Earphones
      try { addBoxItem(document.Support_Form.Product_ID, 'Earphones EP-880', '11156'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Earphones EP-630', '11397'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Earphones EP-480', '11708'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Earphones EP-380', '11229'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 442 :                     // Headsets
      try { addBoxItem(document.Support_Form.Product_ID, 'Headset HE-100', '11023'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Headset HS-300', '4939'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;


    }
    break;

  case 9 :                             // Storage Devices
    switch(parseInt(famID)) {

    case 259 :                     // DVD±RW Drive
      try { addBoxItem(document.Support_Form.Product_ID, 'DVD±RW Dual 8X', '9599'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'DVD±RW Dual 8x8', '10305'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'DVD+RW Dual External 8x8', '10583'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 47 :                     // Combo Drive
      try { addBoxItem(document.Support_Form.Product_ID, 'Combo Drive 52.32.52x/16x', '11712'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Combo Drive 40-12-40/16', '4998'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Combo Drive NS', '9454'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 46 :                     // CD-RW Drive
      try { addBoxItem(document.Support_Form.Product_ID, 'CD-RW External 52-32-52x', '9481'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'CD-RW Blaster 12-10-32', '8'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'CD-RW 52.24.52', '1590'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'CD-RW Blaster 24-10-40 External', '4944'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'CD-RW External 52-24-52x', '9027'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'CD-RW 52-32-52x', '9453'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'CD-RW Blaster 48-12-48 External', '9020'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'CD-RW Blaster 48-12-48', '4941'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 45 :                     // PC-DVD Drive
      try { addBoxItem(document.Support_Form.Product_ID, 'PC-DVD Encore 12x', '6'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'PC-DVD ROM 12x', '1486'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'PC-DVD DVD-ROM 16x', '1490'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'PC-DVD MPEG-1 Decoder Board', '1801'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 44 :                     // CD-ROM Drive
      try { addBoxItem(document.Support_Form.Product_ID, 'CD-ROM Blaster Digital iR52X', '3562'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'CD-ROM Blaster 52X', '11'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '12x CD-ROM Drives', '1485'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '16x CD-ROM Drives', '1489'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '48x CD-ROM Drives', '1548'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '1x & 2x CD-ROM Drives', '1493'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '24x CD-ROM Drives', '1495'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '2x & 3x CD-ROM (SCSI)', '1496'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '32x CD-ROM Drives', '1498'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '36x CD-ROM Drives', '1499'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '40x CD-Rom Drives', '1547'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '4x CD-ROM Drives', '1549'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '6x CD-ROM Drives', '1552'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '8x CD-ROM Drives', '1554'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 49 :                     // Portable Harddisk
      try { addBoxItem(document.Support_Form.Product_ID, 'Storage Blaster Portable Harddisk', '8996'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 265 :                     // Portable Storage
      try { addBoxItem(document.Support_Form.Product_ID, 'ThumbDrive', '10681'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;


    }
    break;

  case 12 :                             // PC Barebone
    switch(parseInt(famID)) {

    case 54 :                     // SLiX PC
      try { addBoxItem(document.Support_Form.Product_ID, 'SLiX PC MPC61Y0', '11766'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;


    }
    break;

  case 11 :                             // Monitors
    switch(parseInt(famID)) {

    case 53 :                     // LCD
      try { addBoxItem(document.Support_Form.Product_ID, '17" TFT LCD Monitor With DVI', '9980'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;


    }
    break;

  case 60 :                             // Video
    switch(parseInt(famID)) {

    case 96 :                     // Video Editing
      try { addBoxItem(document.Support_Form.Product_ID, 'Video Blaster MovieMaker', '13'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;


    }
    break;

  case 13 :                             // Accessories
    switch(parseInt(famID)) {

    case 55 :                     // Sound Blaster
      try { addBoxItem(document.Support_Form.Product_ID, 'Optical Digital I/O Card II', '30'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Sound Blaster MIDI Adapter', '251'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Mini to Standard MIDI Adapter', '252'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Live!Drive II', '9278'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Sound Blaster Audigy Internal Drive', '88'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Sound Blaster Audigy External Drive', '89'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Live! Drive IR', '26'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Live! Drive I', '27'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Remote Controller SBLive', '1816'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 449 :                     // Zen Micro
      try { addBoxItem(document.Support_Form.Product_ID, 'Universal Travel Adapter for Zen Micro', '11711'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Leather Case', '11511'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 483 :                     // Zen Neeon
      try { addBoxItem(document.Support_Form.Product_ID, 'Zen Neeon Stik-On', '12982'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Zen Neeon Leather Case', '12978'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Zen Neeon Universal Travel Adapter', '12979'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 264 :                     // Portable Media Center
      try { addBoxItem(document.Support_Form.Product_ID, 'Zen PMC Docking Station', '10756'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Zen PMC Li-ion Polymer Battery', '10679'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Zen PMC FM Wired Remote', '10663'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 57 :                     // MP3 Players
      try { addBoxItem(document.Support_Form.Product_ID, 'Creative NOMAD JukeBox 3 Car Kit', '4894'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Home Kit - Jukebox 3', '497'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative NOMAD Jukebox 3 Leather Case', '498'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Faceplates - Jukebox 3', '499'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'MuVo Armband', '511'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative NOMAD Jukebox Accessory Kit', '533'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative NOMAD Jukebox Battery Charger Kit', '538'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative NOMAD Jukebox Battery Pack', '539'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Leather Case - Jukebox 2', '562'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Battery - Jukebox 2', '563'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Li-Ion Battery - Jukebox 3', '86'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'FM Wired Remote - Jukebox 3/Jukebox Zen/MuVo\u00B2', '115'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative NOMAD Jukebox Power Adapter', '125'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Cassette Adapter Kit', '401'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Car Kit - Jukebox 3/Jukebox 2/Jukebox Zen/MuVo\u00B2', '496'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Battery Modules - MuVo NX / TX / TX FM', '9217'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Armband - MuVo NX / TX / TX FM', '10126'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 58 :                     // Speaker Systems
      try { addBoxItem(document.Support_Form.Product_ID, 'MT-1100 Speaker Stands', '166'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Headphones HQ-1700', '11164'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Earphones EP-880', '11156'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Headset HE-100', '11023'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'MT-500 Speaker Tripods', '399'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Speaker Extension Cables', '415'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Inspire 5300 Spkr Grilles', '637'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Inspire 5700 Spkr Grilles', '664'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 256 :                     // Wireless
      try { addBoxItem(document.Support_Form.Product_ID, 'Wireless Headset for Bluetooth', '10287'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Headset CB2455', '11394'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Headphones CB2530', '11644'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 59 :                     // Storage
      try { addBoxItem(document.Support_Form.Product_ID, 'S-Video Cable Coupler', '250'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'IDE Cable Set', '255'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Dxr2 Decoder Board', '1648'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Dxr3 Decoder Card', '12'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;


    }
    break;

  case 118 :                             // Digital Cameras
    switch(parseInt(famID)) {

    case 117 :                     // Digital Still Cameras
      try { addBoxItem(document.Support_Form.Product_ID, 'DC-CAM 4200ZS', '10822'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'DC-CAM 3200Z', '9762'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'DC-CAM 3000Z', '9028'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'CardCam', '120'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'CardCam Value', '116'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 309 :                     // Digital Video Cameras
      try { addBoxItem(document.Support_Form.Product_ID, 'DiVi CAM 316', '11175'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;


    }
    break;

  case 10 :                             // Mice & Keyboards
    switch(parseInt(famID)) {

    case 223 :                     // Wired Mice
      try { addBoxItem(document.Support_Form.Product_ID, 'Mouse 5500', '11387'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Mouse 3500', '11388'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Mouse Classic', '4919'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Mouse Optical Lite', '4920'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Mouse Optical 3000', '4924'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Creative Optical Mouse', '262'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Mouse Notebook Optical', '9147'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 224 :                     // Wireless Mice
      try { addBoxItem(document.Support_Form.Product_ID, 'FreePoint Travel', '11165'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'FreePoint Travel Mini', '11166'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'FreePoint 5500', '11178'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'FreePoint 3500', '11386'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Mouse Wireless Optical 5000', '9145'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Mouse Wireless Optical', '263'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Mouse Wireless Optical 3000', '4923'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 227 :                     // Wireless Mice & Keyboards
      try { addBoxItem(document.Support_Form.Product_ID, 'Desktop Wireless 9000 Pro', '11493'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Desktop Wireless 8000', '10104'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Desktop Wireless 6000', '5039'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 228 :                     // Wired PC & MIDI Keyboards
      try { addBoxItem(document.Support_Form.Product_ID, 'Prodikeys DM', '9389'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Prodikeys DM Value', '9600'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Prodikeys', '504'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 52 :                     // Gaming Devices
      try { addBoxItem(document.Support_Form.Product_ID, 'Avant Force NX', '9394'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Cobra Force 3D', '9396'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Gamepad I', '1658'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;


    }
    break;

  case 8 :                             // Musical Keyboards
    switch(parseInt(famID)) {

    case 234 :                     // PC & MIDI Keyboards
      try { addBoxItem(document.Support_Form.Product_ID, 'Prodikeys DM', '9389'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Prodikeys DM Value', '9600'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Prodikeys', '504'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 231 :                     // MIDI Keyboards
      try { addBoxItem(document.Support_Form.Product_ID, 'Creative Blasterkeys', '40'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;


    }
    break;

  case 237 :                             // Creative Professional
    switch(parseInt(famID)) {

    case 239 :                     // Digital Audio Systems
      try { addBoxItem(document.Support_Form.Product_ID, 'E-MU 1820M', '10496'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'E-MU 1212M', '10500'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'E-MU 1820', '10494'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'E-MU 0404', '10498'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 240 :                     // Desktop Sampling Systems
      try { addBoxItem(document.Support_Form.Product_ID, 'Emulator X', '10502'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Emulator X Studio', '10504'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Proteus X', '11074'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 421 :                     // Desktop Sound Modules
      try { addBoxItem(document.Support_Form.Product_ID, 'Proteus X', '11074'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 258 :                     // Accessories and Upgrades
      try { addBoxItem(document.Support_Form.Product_ID, 'Proteus X Software Upgrade', '11073'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Emulator X OS Upgrade', '10225'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Mo\'Phatt X', '11329'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Planet Earth X', '11330'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Virtuoso X', '11332'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Vintage X Pro Collection', '11072'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Street Kits', '11331'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'AudioDock M', '10229'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Audio Dock', '10230'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Beat Shop 2', '10404'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'EDI Cable', '10227'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Sync Daughter Card', '10576'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;


    }
    break;

  case 243 :                             // Wireless
    switch(parseInt(famID)) {

    case 246 :                     // Mice & Keyboards
      try { addBoxItem(document.Support_Form.Product_ID, 'FreePoint 5500', '11178'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'FreePoint 3500', '11386'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Mouse Wireless Optical 5000', '9145'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Desktop Wireless 8000', '10104'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Desktop Wireless 6000', '5039'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Mouse Wireless Optical', '263'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Mouse Wireless Optical 3000', '4923'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 248 :                     // Accessories
      try { addBoxItem(document.Support_Form.Product_ID, 'Headset CB2460', '11238'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Headset CB2455', '11394'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Wireless Headset for Bluetooth', '10287'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Headphones CB2530', '11644'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;


    }
    break;

  case 244 :                             // Notebook Products
    switch(parseInt(famID)) {

    case 250 :                     // PCMCIA Sound Blaster
      try { addBoxItem(document.Support_Form.Product_ID, 'Audigy 2 ZS Notebook', '10769'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 249 :                     // USB Sound Blaster
      try { addBoxItem(document.Support_Form.Product_ID, 'Audigy 2 NX', '9103'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Live! 24-bit External', '10702'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Digital Music LX', '10246'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Digital Music', '154'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Extigy', '585'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 251 :                     // Portable Speaker Systems
      try { addBoxItem(document.Support_Form.Product_ID, 'TravelSound 200', '10164'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'TravelSound i-300', '9022'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'TravelSound MP3', '1874'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 252 :                     // Mice & Keyboards
      try { addBoxItem(document.Support_Form.Product_ID, 'Mouse Wireless NoteBook Optical', '10188'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Mouse Notebook Optical', '9147'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 253 :                     // Web Cameras
      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam Live! Ultra for Notebooks', '11491'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam Notebook', '629'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'WebCam Mobile', '4890'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;


    }
    break;

  case 6 :                             // Graphics
    switch(parseInt(famID)) {

    case 37 :                     // ATI Radeon 9000 series
      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster RX9250', '11489'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster RX9250 Xtreme', '11490'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 230 :                     // 3D Labs
      try { addBoxItem(document.Support_Form.Product_ID, 'Graphics Blaster Picture Perfect', '164'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 99 :                     // NVIDIA GeForce
      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster GeForce', '1500'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster GeForce 2', '1501'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster GeForce Pro', '1505'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster GeForce2 ULTRA 64MB AGP', '1512'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 98 :                     // NVIDIA Riva TNT Series
      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster RIVA TNT2 Pro', '1527'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Graphics Blaster RIVA128ZX', '1689'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster Riva TNT2', '4841'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster RIVA TNT2 Value', '4842'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster TNT2 Ultra', '4843'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 73 :                     // 3D Blaster
      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster 4 Titanium 4200', '1516'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster 5 RX9700 Pro', '1524'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster 4 MX440', '1539'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster 5 RX9000 64MB', '1540'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster 5 RX9000 Pro', '1541'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster 4 MX420', '4869'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster 5 RX9800 Pro', '4917'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster 4 MX460', '4969'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster 5 RX9600', '4973'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster 5 RX9000 Pro 128MB', '8995'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster 5 RX9200 SE', '9024'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster 5 RX9600 Pro', '9576'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster 5 RX9600 XT', '10311'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster 5 RX9600 SE', '10335'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster Savage 4', '1536'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster VLB', '1537'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster Voodoo 2', '1538'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster PCI', '1523'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster Banshee', '1506'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, '3D Blaster Banshee AGP', '1507'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;


    }
    break;

  case 7 :                             // Modems & Networking
    switch(parseInt(famID)) {

    case 41 :                     // Wireless
      try { addBoxItem(document.Support_Form.Product_ID, 'USB Adapter CB2431', '10863'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Network Blaster Wireless PCMCIA Card', '3868'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 42 :                     // Broadband
      try { addBoxItem(document.Support_Form.Product_ID, 'Broadband Blaster DSL Router 8015U', '11176'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Broadband Blaster Router 8110', '10280'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Broadband Blaster ADSL Bridge ', '4873'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Broadband Blaster USB Modem', '4871'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Broadband Blaster DSL', '4921'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 40 :                     // Analog
      try { addBoxItem(document.Support_Form.Product_ID, 'Modem Blaster PCMCIA', '24'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Modem Blaster V.92 PCI', '52'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Modem Blaster V.92 Serial', '258'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Modem Blaster V.92 USB', '266'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Modem 56k Internal', '1715'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Modem Blaster Flash 56II ISA', '18'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Modem Blaster V.90 ISA', '19'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Modem Blaster Flash 56 PCI', '21'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Modem Blaster V.90 USB', '22'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Modem Blaster V.90 External', '23'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Modem Blaster USB (DE5675)', '8992'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Modem Blaster USB (DE5673)', '8991'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Modem Blaster PCI (DI5663)', '4999'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Modem Blaster PCI (DI5656)', '8988'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Modem Blaster PCI (DI5655)', '8989'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Modem Blaster External (DE5625)', '8990'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Modem Blaster 28.8 External', '5000'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'CT5451 Modem Blaster Voice PnP', '5001'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'Phone Blaster', '1809'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;


    }
    break;

  case 106 :                             // Software
    switch(parseInt(famID)) {

    case 241 :                     // HansVision DXT
      try { addBoxItem(document.Support_Form.Product_ID, 'HansVision DXT 2005 Edition', '12218'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 242 :                     // Children\'s Multimedia Educational
      try { addBoxItem(document.Support_Form.Product_ID, 'WaWaYaYa Happy Mandarin Series', '11269'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      try { addBoxItem(document.Support_Form.Product_ID, 'WaWaYaYa Happy English Series', '4932'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 107 :                     // HansVision
      try { addBoxItem(document.Support_Form.Product_ID, 'HansVision DXT', '4928'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 108 :                     // WaWaYaYa
      try { addBoxItem(document.Support_Form.Product_ID, 'WaWaYaYa Comprehensive Ability Series', '4930'); } catch(e) {addBoxItem(document.Support_Form.Product_ID, '1', '2');  } //

      break;

    case 109 :                     // Others

      break;


    }
    break;

  }
  //                addBoxItem(document.Support_Form.Product_ID, 'Zen Portable Media Center', 'DUMMYPREFIX_ZenPMC_Temp|9882');
}

try
{
  productList(0,0);
}
catch(e)
{
}

reportCompare(expect, actual, summary);
