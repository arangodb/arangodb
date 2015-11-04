/*global document, $, _ */

////////////////////////////////////////////////////////////////////////////////
/// @brief Graph functionality
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var uiComponentsHelper = uiComponentsHelper || {};

(function componentsHelper() {
  "use strict";

  uiComponentsHelper.createButton = function(list, title, prefix, callback) {
    var li = document.createElement("li"),
      button = document.createElement("button");
    li.className = "graph_control " + prefix;
    li.id = prefix;
    li.appendChild(button);
    button.className = "button-primary gv_dropdown_entry";
    button.appendChild(document.createTextNode(title));
    list.appendChild(li);
    button.id = prefix + "_button";
    button.onclick = callback;
  };

  uiComponentsHelper.createIconButton = function(iconInfo, prefix, callback) {
    var button = document.createElement("div"),
        icon = document.createElement("h6"),
        label = document.createElement("h6");
    button.className = "gv_action_button";
    button.id = prefix;
    button.onclick = function() {
      $(".gv_action_button").each(function(i, btn) {
        $(btn).toggleClass("active", false);
      });

      if (button.id === "control_event_new_node") {
        $('.node').css('cursor', 'pointer');
        $('.gv-background').css('cursor', 'copy');
      }
      else if (button.id === "control_event_drag") {
        $('.node').css('cursor', '-webkit-grabbing');
        $('.gv-background').css('cursor', 'default');
      }
      else if (button.id === "control_event_expand") {
        $('.node').css('cursor', 'grabbing');
        $('.gv-background').css('cursor', 'default');
      }
      else if (button.id === "control_event_view") {
        $('.node').css('cursor', '-webkit-zoom-in');
        $('.gv-background').css('cursor', 'default');
      }
      else if (button.id === "control_event_edit") {
        $('.gv-background .node').css('cursor', 'context-menu');
        $('.gv-background').css('cursor', 'default');
      }
      else if (button.id === "control_event_connect") {
        $('.node').css('cursor', 'ne-resize');
        $('.gv-background').css('cursor', 'default');
      }
      else if (button.id === "control_event_delete") {
        $('.node').css('cursor', 'pointer');
        $('.gv-background').css('cursor', 'default');
      }

      $(button).toggleClass("active", true);
      callback();
    };
    icon.className = "fa gv_icon_icon fa-" + iconInfo.icon;
    icon.title = iconInfo.title;
    label.className = "gv_button_title";
    button.appendChild(icon);
    button.appendChild(label);
    label.appendChild(document.createTextNode(iconInfo.title));
    return button;
  };

}());
