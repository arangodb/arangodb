/*jshint unused: false */
/*global window, $, document, _ */

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

    setCheckboxStatus: function(id) {
      _.each($(id).find('ul').find('li'), function(element) {
         if (!$(element).hasClass("nav-header")) {
           if ($(element).find('input').attr('checked')) {
             if ($(element).find('i').hasClass('css-round-label')) {
               $(element).find('i').addClass('fa-dot-circle-o');
             }
             else {
               $(element).find('i').addClass('fa-check-square-o');
             }
           }
           else {
             if ($(element).find('i').hasClass('css-round-label')) {
               $(element).find('i').addClass('fa-circle-o');
             }
             else {
               $(element).find('i').addClass('fa-square-o');
             }
           }
         }
      });
    },

    calculateCenterDivHeight: function() {
      var navigation = $('.navbar').height();
      var footer = $('.footer').height();
      var windowHeight = $(window).height();

      return windowHeight - footer - navigation - 110;
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
          label: "Save",
          letter: "Ctrl + Return, CMD + Return"
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

    arangoNotification: function (title, content, info) {
      window.App.notificationList.add({title:title, content: content, info: info, type: 'success'});
    },

    arangoError: function (title, content, info) {
      window.App.notificationList.add({title:title, content: content, info: info, type: 'error'});
    },

    openDocEditor: function (id, type, callback) {
      var ids = id.split("/"),
      self = this;

      var docFrameView = new window.DocumentView({
        collection: window.App.arangoDocumentStore
      });

      docFrameView.breadcrumb = function(){};

      docFrameView.colid = ids[0];
      docFrameView.docid = ids[1];

      docFrameView.el = '.arangoFrame .innerDiv';
      docFrameView.render();
      docFrameView.setType(type);

      //remove header
      $('.arangoFrame .headerBar').remove();
      //append close button
      $('.arangoFrame .outerDiv').prepend('<i class="fa fa-times"></i>');
      //add close events
      $('.arangoFrame .outerDiv').click(function() {
        self.closeDocEditor();
      });
      $('.arangoFrame .innerDiv').click(function(e) {
        e.stopPropagation();
      });
      $('.fa-times').click(function() {
        self.closeDocEditor();
      });

      $('.arangoFrame').show();
       
      docFrameView.customView = true;
      docFrameView.customDeleteFunction = function() {
        window.modalView.hide();
        $('.arangoFrame').hide();
        //callback();
      };

      $('.arangoFrame #deleteDocumentButton').click(function(){
        docFrameView.deleteDocumentModal();
      });
      $('.arangoFrame #saveDocumentButton').click(function(){
        docFrameView.saveDocument();
      });
      $('.arangoFrame #deleteDocumentButton').css('display', 'none');
    },

    closeDocEditor: function () {
      $('.arangoFrame .outerDiv .fa-times').remove();
      $('.arangoFrame').hide();
    },

    addAardvarkJob: function (object, callback) {
      $.ajax({
          cache: false,
          type: "POST",
          url: "/_admin/aardvark/job",
          data: JSON.stringify(object),
          contentType: "application/json",
          processData: false,
          success: function (data) {
            if (callback) {
              callback(false, data);
            }
          },
          error: function(data) {
            if (callback) {
              callback(true, data);
            }
          }
      });
    },

    deleteAardvarkJob: function (id, callback) {
      $.ajax({
          cache: false,
          type: "DELETE",
          url: "/_admin/aardvark/job/" + encodeURIComponent(id),
          contentType: "application/json",
          processData: false,
          success: function (data) {
            if (callback) {
              callback(false, data);
            }
          },
          error: function(data) {
            if (callback) {
              callback(true, data);
            }
          }
      });
    },

    deleteAllAardvarkJobs: function (callback) {
      $.ajax({
          cache: false,
          type: "DELETE",
          url: "/_admin/aardvark/job",
          contentType: "application/json",
          processData: false,
          success: function (data) {
            if (callback) {
              callback(false, data);
            }
          },
          error: function(data) {
            if (callback) {
              callback(true, data);
            }
          }
      });
    },

    getAardvarkJobs: function (callback) {
      var result;

      $.ajax({
          cache: false,
          type: "GET",
          url: "/_admin/aardvark/job",
          contentType: "application/json",
          processData: false,
          async: false,
          success: function (data) {
            if (callback) {
              callback(false, data);
            }
            result = data;
          },
          error: function(data) {
            if (callback) {
              callback(true, data);
            }
          }
      });
      return result;
    },

    getPendingJobs: function() {
      var result; 

      $.ajax({
          cache: false,
          type: "GET",
          url: "/_api/job/pending",
          contentType: "application/json",
          processData: false,
          async: false,
          success: function (data) {
            result = data;
          },
          error: function(data) {
            console.log("pending jobs error: " + data);
          }
      });
      return result;
    },

    syncAndReturnUninishedAardvarkJobs: function(type) {

      var AaJobs = this.getAardvarkJobs(),
      pendingJobs = this.getPendingJobs(),
      array = [];

      if (pendingJobs.length > 0) {
        _.each(AaJobs, function(aardvark) {
          if (aardvark.type === type || aardvark.type === undefined) {

             var found = false; 
            _.each(pendingJobs, function(pending) {
              if (aardvark.id === pending) {
                found = true;
              } 
            });

            if (found) {
              array.push({
                collection: aardvark.collection,
                id: aardvark.id,
                type: aardvark.type,
                desc: aardvark.desc 
              });
            }
            else {
              window.arangoHelper.deleteAardvarkJob(aardvark.id);
            }
          }
        });
      }
      else {
        this.deleteAllAardvarkJobs(); 
      }

      return array;
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
