/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, $  */

(function() {
  "use strict";
  window.versionHelper = {
    fromString: function (s) {
      var parts = s.replace(/-[a-zA-Z0-9_\-]*$/g, '').split('.');
      return {
        major: parseInt(parts[0], 10) || 0,
        minor: parseInt(parts[1], 10) || 0,
        patch: parseInt(parts[2], 10) || 0
      };
    },
    toString: function (v) {
      return v.major + '.' + v.minor + '.' + v.patch;
    },
    toStringMainLine: function (v) {
      return v.major + '.' + v.minor;
    },
    compareVersions: function (l, r) {
      if (l.major === r.major) {
        if (l.minor === r.minor) {
          if (l.patch === r.patch) {
            return 0;
          }
          return l.patch - r.patch;
        }
        return l.minor - r.minor;
      }
      return l.major - r.major;
    },
    compareVersionStrings: function (l, r) {
      l = window.versionHelper.fromString(l);
      r = window.versionHelper.fromString(r);
      return window.versionHelper.compareVersions(l, r);
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
        error: function(data) {
          returnVal = false;
        }
      });
      return returnVal;
    },

    databaseAllowed: function () {
      var currentDB = this.currentDatabase(),
        returnVal = false;
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_db/"+encodeURIComponent(currentDB)+"/_api/database/",
        contentType: "application/json",
        processData: false,
        async: false,
        success: function(data) {
          returnVal = true;
        },
        error: function(data) {
          returnVal = false;
        }
      });
      return returnVal;
    },

    removeNotifications: function () {
      $.gritter.removeAll();
      this.lastNotificationMessage = null;
    },
    arangoNotification: function (message, time) {
      var returnVal = false;
      $.gritter.add({
        title: "Notice:",
        text: message,
        time: time || 3000,
        before_open: function(){
          returnVal = true;
        }
      });
      this.lastNotificationMessage = null;

      return returnVal;
    },
    arangoError: function (message) {
      var returnVal = false;
      $.gritter.add({
        title: "Error:",
        text: message,
        sticky: true,
        before_open: function(){
          if (this.lastNotificationMessage === message) {
            // prevent display the same message over & over
            return false;
          }
          if($('.gritter-item-wrapper').length === 3) {
            // not more than 3 messages at once
            return false;
          }
          this.lastNotificationMessage = message;
          returnVal = true;
        },
        before_close: function(){
          // reset last text when closing a specific message
          this.lastNotificationMessage = null;
        }
      });
      return returnVal;
    },
    getRandomToken: function () {
      return Math.round(new Date().getTime());
    },

    isSystemAttribute: function (val) {
      var a = this.systemAttributes();
      return a[val];
    },

    isSystemCollection: function (val) {
      //return val && val.name && val.name.substr(0, 1) === '_';
      return val.substr(0, 1) === '_';
    },

    collectionApiType: function (identifier, refresh) {
      // set "refresh" to disable caching collection type
      if (refresh || this.CollectionTypes[identifier] === undefined) {
        this.CollectionTypes[identifier] = window.arangoDocumentStore
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

      if (val.name.substr(0, 1) === '_') {
        type += " (system)";
      }

      return type;
    },

    FormatJSON: function (oData, sIndent) {
      var self = this;
      var sHTML, iCount;
      if (sIndent === undefined) {
        sIndent = "";
      }
      var sIndentStyle = " ";
      var sDataType = self.RealTypeOf(oData);

      if (sDataType === "array") {
        if (oData.length === 0) {
          return "[]";
        }
        sHTML = "[";
      } else {
        iCount = 0;
        $.each(oData, function() {
          iCount++;
          return;
        });
        if (iCount === 0) { // object is empty
          return "{}";
        }
        sHTML = "{";
      }

      iCount = 0;
      $.each(oData, function(sKey, vValue) {
        if (iCount > 0) {
          sHTML += ",";
        }
        if (sDataType === "array") {
          sHTML += ("\n" + sIndent + sIndentStyle);
        } else {
          sHTML += ("\n" + sIndent + sIndentStyle + JSON.stringify(sKey) + ": ");
        }

        // display relevant data type
        switch (self.RealTypeOf(vValue)) {
          case "array":
            case "object":
            sHTML += self.FormatJSON(vValue, (sIndent + sIndentStyle));
          break;
          case "boolean":
            case "number":
            sHTML += vValue.toString();
          break;
          case "null":
            sHTML += "null";
          break;
          case "string":
            sHTML += "\"" + vValue.replace(/\\/g, "\\\\").replace(/"/g, "\\\"") + "\"";
          break;
          default:
            sHTML += ("TYPEOF: " + typeof vValue);
        }
        // loop
        iCount++;
      });

      // close object
      if (sDataType === "array") {
        sHTML += ("\n" + sIndent + "]");
      } else {
        sHTML += ("\n" + sIndent + "}");
      }

      // return
      return sHTML;
    },

    RealTypeOf: function (v) {
      if (typeof v === "object") {
        if (v === null) {
          return "null";
        }
        var array = [];
        if (v.constructor === array.constructor) {
          return "array";
        }
        var date = new Date();
        if (v.constructor === date.constructor) {
          return "date";
        }
        var regexp = new RegExp();
        if (v.constructor === regexp.constructor) {
          return "regex";
        }
        return "object";
      }
      return typeof v;
    }

  };
}());
