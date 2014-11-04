/*jshint unused: false */
/*global window, $, document */

(function() {
  "use strict";
  var isCoordinator;

  window.isCoordinator = function() {
    if (isCoordinator === undefined) {
      $.ajax(
        "cluster/amICoordinator",
        {
          async: false,
          success: function(d) {
            isCoordinator = d;
          }
        }
      );
    }
    return isCoordinator;
  };

  window.versionHelper = {
    fromString: function (s) {
      var parts = s.replace(/-[a-zA-Z0-9_\-]*$/g, '').split('.');
      return {
        major: parseInt(parts[0], 10) || 0,
        minor: parseInt(parts[1], 10) || 0,
        patch: parseInt(parts[2], 10) || 0,
        toString: function() {
          return this.major + "." + this.minor + "." + this.patch;
        }
      };
    },
    toString: function (v) {
      return v.major + '.' + v.minor + '.' + v.patch;
    }
  };

  window.arangoHelper = {
    lastNotificationMessage: null,

    CollectionTypes: {},
    systemAttributes: function () {
      return {
        '_id' : true,
        '_rev' : true,
        '_key' : true,
        '_bidirectional' : true,
        '_vertices' : true,
        '_from' : true,
        '_to' : true,
        '$id' : true
      };
    },

    fixTooltips: function (selector, placement) {
      $(selector).tooltip({
        placement: placement,
        hide: false,
        show: false
      });
    },

    currentDatabase: function () {
      var returnVal = false;
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_api/database/current",
        contentType: "application/json",
        processData: false,
        async: false,
        success: function(data) {
          returnVal = data.result.name;
        },
        error: function() {
          returnVal = false;
        }
      });
      return returnVal;
    },

    allHotkeys: {
      global: {
        name: "Site wide",
        content: [{
          label: "scroll up",
          letter: "j"
        },{
          label: "scroll down",
          letter: "k"
        }]
      },
      jsoneditor: {
        name: "AQL editor",
        content: [{
          label: "Submit",
          letter: "Ctrl + Return"
        },{
          label: "Toggle comments",
          letter: "Ctrl + Shift + C"
        },{
          label: "Undo",
          letter: "Ctrl + Z"
        },{
          label: "Redo",
          letter: "Ctrl + Shift + Z"
        }]
      },
      doceditor: {
        name: "Document editor",
        content: [{
          label: "Insert",
          letter: "Ctrl + Insert"
        },{
          label: "Append",
          letter: "Ctrl + Shift + Insert"
        },{
          label: "Duplicate",
          letter: "Ctrl + D"
        },{
          label: "Remove",
          letter: "Ctrl + Delete"
        }]
      },
      modals: {
        name: "Modal",
        content: [{
          label: "Submit",
          letter: "Return"
        },{
          label: "Close",
          letter: "Esc"
        },{
          label: "Navigate buttons",
          letter: "Arrow keys"
        },{
          label: "Navigate content",
          letter: "Tab"
        }]
      }
    },

    hotkeysFunctions: {
      scrollDown: function () {
        window.scrollBy(0,180);
      },
      scrollUp: function () {
        window.scrollBy(0,-180);
      },
      showHotkeysModal: function () {
        var buttons = [],
        content = window.arangoHelper.allHotkeys;

        window.modalView.show("modalHotkeys.ejs", "Keyboard Shortcuts", buttons, content);
      }
    },

    enableKeyboardHotkeys: function (enable) {
      var hotkeys = window.arangoHelper.hotkeysFunctions;
      if (enable === true) {
        $(document).on('keydown', null, 'j', hotkeys.scrollDown);
        $(document).on('keydown', null, 'k', hotkeys.scrollUp);
      }
    },

    databaseAllowed: function () {
      var currentDB = this.currentDatabase(),
      returnVal = false;
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_db/"+ encodeURIComponent(currentDB) + "/_api/database/",
        contentType: "application/json",
        processData: false,
        async: false,
        success: function() {
          returnVal = true;
        },
        error: function() {
          returnVal = false;
        }
      });
      return returnVal;
    },

    arangoNotification: function (title, content) {
      window.App.notificationList.add({title:title, content: content});
    },

    arangoError: function (title, content) {
      window.App.notificationList.add({title:title, content: content});
    },

    getRandomToken: function () {
      return Math.round(new Date().getTime());
    },

    isSystemAttribute: function (val) {
      var a = this.systemAttributes();
      return a[val];
    },

    isSystemCollection: function (val) {
      return val.name.substr(0, 1) === '_';
      // the below code is completely inappropriate as it will
      // load the collection just for the check whether it
      // is a system collection. as a consequence, the below
      // code would load ALL collections when the web interface
      // is called
      /*
         var returnVal = false;
         $.ajax({
type: "GET",
url: "/_api/collection/" + encodeURIComponent(val) + "/properties",
contentType: "application/json",
processData: false,
async: false,
success: function(data) {
returnVal = data.isSystem;
},
error: function(data) {
returnVal = false;
}
});
return returnVal;
*/
    },

    setDocumentStore : function (a) {
      this.arangoDocumentStore = a;
    },

    collectionApiType: function (identifier, refresh) {
      // set "refresh" to disable caching collection type
      if (refresh || this.CollectionTypes[identifier] === undefined) {
        this.CollectionTypes[identifier] = this.arangoDocumentStore
        .getCollectionInfo(identifier).type;
      }
      if (this.CollectionTypes[identifier] === 3) {
        return "edge";
      }
      return "document";
    },

    collectionType: function (val) {
      if (! val || val.name === '') {
        return "-";
      }
      var type;
      if (val.type === 2) {
        type = "document";
      }
      else if (val.type === 3) {
        type = "edge";
      }
      else {
        type = "unknown";
      }

      if (this.isSystemCollection(val)) {
        type += " (system)";
      }

      return type;
    },

    formatDT: function (dt) {
      var pad = function (n) {
        return n < 10 ? '0' + n : n;
      };

      return dt.getUTCFullYear() + '-'
      + pad(dt.getUTCMonth() + 1) + '-'
      + pad(dt.getUTCDate()) + ' '
      + pad(dt.getUTCHours()) + ':'
      + pad(dt.getUTCMinutes()) + ':'
      + pad(dt.getUTCSeconds());
    },

    escapeHtml: function (val) {
      // HTML-escape a string
      return String(val).replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#39;');
    }

  };
}());
