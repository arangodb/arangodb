/* jshint unused: false */
/* global Blob, window, sigma, $, Tippy, document, _, arangoHelper, frontendConfig, arangoHelper, localStorage */

(function () {
  'use strict';
  var isCoordinator = null;

  window.isCoordinator = function (callback) {
    if (isCoordinator === null) {
      $.ajax(
        'cluster/amICoordinator',
        {
          async: true,
          success: function (d) {
            isCoordinator = d;
            callback(false, d);
          },
          error: function (d) {
            isCoordinator = d;
            callback(true, d);
          }
        }
      );
    } else {
      callback(false, isCoordinator);
    }
  };

  window.versionHelper = {
    fromString: function (s) {
      var parts = s.replace(/-[a-zA-Z0-9_-]*$/g, '').split('.');
      return {
        major: parseInt(parts[0], 10) || 0,
        minor: parseInt(parts[1], 10) || 0,
        patch: parseInt(parts[2], 10) || 0,
        toString: function () {
          return this.major + '.' + this.minor + '.' + this.patch;
        }
      };
    },
    toString: function (v) {
      return v.major + '.' + v.minor + '.' + v.patch;
    }
  };

  window.arangoHelper = {

    alphabetColors: {
      a: 'rgb(0,0,180)',
      b: 'rgb(175,13,102)',
      c: 'rgb(146,248,70)',
      d: 'rgb(255,200,47)',
      e: 'rgb(255,118,0)',
      f: 'rgb(185,185,185)',
      g: 'rgb(235,235,222)',
      h: 'rgb(100,100,100)',
      i: 'rgb(255,255,0)',
      j: 'rgb(55,19,112)',
      k: 'rgb(255,255,150)',
      l: 'rgb(202,62,94)',
      m: 'rgb(205,145,63)',
      n: 'rgb(12,75,100)',
      o: 'rgb(255,0,0)',
      p: 'rgb(175,155,50)',
      q: 'rgb(0,0,0)',
      r: 'rgb(37,70,25)',
      s: 'rgb(121,33,135)',
      t: 'rgb(83,140,208)',
      u: 'rgb(0,154,37)',
      v: 'rgb(178,220,205)',
      w: 'rgb(255,152,213)',
      x: 'rgb(0,0,74)',
      y: 'rgb(175,200,74)',
      z: 'rgb(63,25,12)'
    },

    statusColors: {
      fatal: '#ad5148',
      info: 'rgb(88, 214, 141)',
      error: 'rgb(236, 112, 99)',
      warning: '#ffb075',
      debug: 'rgb(64, 74, 83)'
    },

    getCurrentJwt: function () {
      return localStorage.getItem('jwt');
    },

    getCurrentJwtUsername: function () {
      return localStorage.getItem('jwtUser');
    },

    setCurrentJwt: function (jwt, username) {
      localStorage.setItem('jwt', jwt);
      localStorage.setItem('jwtUser', username);
    },

    checkJwt: function () {
      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/version'),
        contentType: 'application/json',
        processData: false,
        async: true,
        error: function (jqXHR) {
          if (jqXHR.status === 401) {
            window.App.navigate('login', {trigger: true});
          }
        }
      });
    },

    getCoordinatorShortName: function (id) {
      var shortName;
      if (window.clusterHealth) {
        _.each(window.clusterHealth, function (value, key) {
          if (id === key) {
            shortName = value.ShortName;
          }
        });
      }
      return shortName;
    },

    getDatabaseShortName: function (id) {
      return this.getCoordinatorShortName(id);
    },

    getDatabaseServerId: function (shortname) {
      var id;
      if (window.clusterHealth) {
        _.each(window.clusterHealth, function (value, key) {
          if (shortname === value.ShortName) {
            id = key;
          }
        });
      }
      return id;
    },

    lastNotificationMessage: null,

    CollectionTypes: {},
    systemAttributes: function () {
      return {
        '_id': true,
        '_rev': true,
        '_key': true,
        '_bidirectional': true,
        '_vertices': true,
        '_from': true,
        '_to': true,
        '$id': true
      };
    },

    getCurrentSub: function () {
      return window.App.naviView.activeSubMenu;
    },

    parseError: function (title, err) {
      var msg;

      try {
        msg = JSON.parse(err.responseText).errorMessage;
      } catch (e) {
        msg = e;
      }

      this.arangoError(title, msg);
    },

    setCheckboxStatus: function (id) {
      _.each($(id).find('ul').find('li'), function (element) {
        if (!$(element).hasClass('nav-header')) {
          if ($(element).find('input').attr('checked')) {
            if ($(element).find('i').hasClass('css-round-label')) {
              $(element).find('i').addClass('fa-dot-circle-o');
            } else {
              $(element).find('i').addClass('fa-check-square-o');
            }
          } else {
            if ($(element).find('i').hasClass('css-round-label')) {
              $(element).find('i').addClass('fa-circle-o');
            } else {
              $(element).find('i').addClass('fa-square-o');
            }
          }
        }
      });
    },

    parseInput: function (element) {
      var parsed;
      var string = $(element).val();

      try {
        parsed = JSON.parse(string);
      } catch (e) {
        parsed = string;
      }

      return parsed;
    },

    calculateCenterDivHeight: function () {
      var navigation = $('.navbar').height();
      var footer = $('.footer').height();
      var windowHeight = $(window).height();

      return windowHeight - footer - navigation - 110;
    },

    createTooltips: function (selector, position) {
      var self = this;

      var settings = {
        arrow: true,
        animation: 'fade',
        animateFill: false,
        multiple: false,
        hideDuration: 1
      };

      if (position) {
        settings.position = position;
      }

      if (!selector) {
        selector = '.tippy';
      }

      if (typeof selector === 'object') {
        _.each(selector, function (elem) {
          self.lastTooltips = new Tippy(elem, settings);
        });
      } else {
        if (selector.indexOf(',') > -1) {
          var selectors = selector.split(',');
          _.each(selectors, function (elem) {
            self.lastTooltips = new Tippy(elem, settings);
          });
        }
        this.lastTooltips = new Tippy(selector, settings);
      }
    },

    fixTooltips: function (selector, placement) {
      arangoHelper.createTooltips(selector, placement);
      /*
      $(selector).tooltip({
        placement: placement,
        hide: false,
        show: false
      });
      */
    },

    currentDatabase: function (callback) {
      if (frontendConfig.db) {
        callback(false, frontendConfig.db);
      } else {
        callback(true, undefined);
      }
      return frontendConfig.db;
    },

    allHotkeys: {
      /*
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
      */
      jsoneditor: {
        name: 'AQL editor',
        content: [{
          label: 'Execute Query',
          letter: 'Ctrl/Cmd + Return'
        }, {
          label: 'Execute Selected Query',
          letter: 'Ctrl/Cmd + Alt + Return'
        }, {
          label: 'Explain Query',
          letter: 'Ctrl/Cmd + Shift + Return'
        }, {
          label: 'Save Query',
          letter: 'Ctrl/Cmd + Shift + S'
        }, {
          label: 'Open search',
          letter: 'Ctrl + Space'
        }, {
          label: 'Toggle comments',
          letter: 'Ctrl/Cmd + Shift + C'
        }, {
          label: 'Undo',
          letter: 'Ctrl/Cmd + Z'
        }, {
          label: 'Redo',
          letter: 'Ctrl/Cmd + Shift + Z'
        }, {
          label: 'Increase Font Size',
          letter: 'Shift + Alt + Up'
        }, {
          label: 'Decrease Font Size',
          letter: 'Shift + Alt + Down'
        }]
      },
      doceditor: {
        name: 'Document editor',
        content: [{
          label: 'Insert',
          letter: 'Ctrl + Insert'
        }, {
          label: 'Save',
          letter: 'Ctrl + Return, Cmd + Return'
        }, {
          label: 'Append',
          letter: 'Ctrl + Shift + Insert'
        }, {
          label: 'Duplicate',
          letter: 'Ctrl + D'
        }, {
          label: 'Remove',
          letter: 'Ctrl + Delete'
        }]
      },
      modals: {
        name: 'Modal',
        content: [{
          label: 'Submit',
          letter: 'Return'
        }, {
          label: 'Close',
          letter: 'Esc'
        }, {
          label: 'Navigate buttons',
          letter: 'Arrow keys'
        }, {
          label: 'Navigate content',
          letter: 'Tab'
        }]
      }
    },

    hotkeysFunctions: {
      scrollDown: function () {
        window.scrollBy(0, 180);
      },
      scrollUp: function () {
        window.scrollBy(0, -180);
      },
      showHotkeysModal: function () {
        var buttons = [];
        var content = window.arangoHelper.allHotkeys;

        window.modalView.show('modalHotkeys.ejs', 'Keyboard Shortcuts', buttons, content);
      }
    },

    // object: {"name": "Menu 1", func: function(), active: true/false }
    buildSubNavBar: function (menuItems) {
      $('#subNavigationBar .bottom').html('');
      var cssClass;

      _.each(menuItems, function (menu, name) {
        cssClass = '';

        if (menu.active) {
          cssClass += ' active';
        }
        if (menu.disabled) {
          cssClass += ' disabled';
        }

        $('#subNavigationBar .bottom').append(
          '<li class="subMenuEntry ' + cssClass + '"><a>' + name + '</a></li>'
        );
        if (!menu.disabled) {
          $('#subNavigationBar .bottom').children().last().bind('click', function () {
            window.App.navigate(menu.route, {trigger: true});
          });
        }
      });
    },

    buildUserSubNav: function (username, activeKey) {
      var menus = {
        General: {
          route: '#user/' + encodeURIComponent(username)
        },
        Permissions: {
          route: '#user/' + encodeURIComponent(username) + '/permission'
        }
      };

      menus[activeKey].active = true;
      this.buildSubNavBar(menus);
    },

    buildGraphSubNav: function (graph, activeKey) {
      var menus = {
        Content: {
          route: '#graph/' + encodeURIComponent(graph)
        },
        Settings: {
          route: '#graph/' + encodeURIComponent(graph) + '/settings'
        }
      };

      menus[activeKey].active = true;
      this.buildSubNavBar(menus);
    },

    buildNodeSubNav: function (node, activeKey, disabled) {
      var menus = {
        Dashboard: {
          route: '#node/' + encodeURIComponent(node)
        }
      /*
      Logs: {
        route: '#nLogs/' + encodeURIComponent(node),
        disabled: true
      }
         */
      };

      menus[activeKey].active = true;
      menus[disabled].disabled = true;
      this.buildSubNavBar(menus);
    },

    buildNodesSubNav: function (activeKey, disabled) {
      var menus = {
        Overview: {
          route: '#nodes'
        },
        Shards: {
          route: '#shards'
        }
      };

      menus[activeKey].active = true;
      if (disabled) {
        menus[disabled].disabled = true;
      }
      this.buildSubNavBar(menus);
    },

    scaleability: undefined,

    /*
    //nav for cluster/nodes view
    buildNodesSubNav: function(type) {

      //if nothing is set, set default to coordinator
      if (type === undefined) {
        type = 'coordinator'
      }

      if (this.scaleability === undefined) {
        var self = this

        $.ajax({
          type: "GET",
          cache: false,
          url: arangoHelper.databaseUrl("/_admin/cluster/numberOfServers"),
          contentType: "application/json",
          processData: false,
          success: function(data) {
            if (data.numberOfCoordinators !== null && data.numberOfDBServers !== null) {
              self.scaleability = true
              self.buildNodesSubNav(type)
            }
            else {
              self.scaleability = false
            }
          }
        })
      }

      var menus = {
        Coordinators: {
          route: '#cNodes'
        },
        DBServers: {
          route: '#dNodes'
        }
      }

      menus.Scale = {
        route: '#sNodes',
        disabled: true
      }

      if (type === 'coordinator') {
        menus.Coordinators.active = true
      }
      else if (type === 'scale') {
        if (this.scaleability === true) {
          menus.Scale.active = true
        }
        else {
          window.App.navigate('#nodes', {trigger: true})
        }
      }
      else {
        menus.DBServers.active = true
      }

      if (this.scaleability === true) {
        menus.Scale.disabled = false
      }

      this.buildSubNavBar(menus)
    },
    */

    // nav for collection view
    buildCollectionSubNav: function (collectionName, activeKey) {
      var defaultRoute = '#collection/' + encodeURIComponent(collectionName);

      var menus = {
        Content: {
          route: defaultRoute + '/documents/1'
        },
        Indexes: {
          route: '#cIndices/' + encodeURIComponent(collectionName)
        },
        Info: {
          route: '#cInfo/' + encodeURIComponent(collectionName)
        },
        Settings: {
          route: '#cSettings/' + encodeURIComponent(collectionName)
        }
      };

      menus[activeKey].active = true;
      this.buildSubNavBar(menus);
    },

    enableKeyboardHotkeys: function (enable) {
      var hotkeys = window.arangoHelper.hotkeysFunctions;
      if (enable === true) {
        $(document).on('keydown', null, 'j', hotkeys.scrollDown);
        $(document).on('keydown', null, 'k', hotkeys.scrollUp);
      }
    },

    databaseAllowed: function (callback) {
      var dbCallback = function (error, db) {
        if (error) {
          arangoHelper.arangoError('', '');
        } else {
          $.ajax({
            type: 'GET',
            cache: false,
            url: this.databaseUrl('/_api/database/', db),
            contentType: 'application/json',
            processData: false,
            success: function () {
              callback(false, true);
            },
            error: function () {
              callback(true, false);
            }
          });
        }
      }.bind(this);

      this.currentDatabase(dbCallback);
    },

    arangoNotification: function (title, content, info) {
      window.App.notificationList.add({title: title, content: content, info: info, type: 'success'});
    },

    arangoError: function (title, content, info) {
      if (!$('#offlinePlaceholder').is(':visible')) {
        window.App.notificationList.add({title: title, content: content, info: info, type: 'error'});
      }
    },

    arangoWarning: function (title, content, info) {
      window.App.notificationList.add({title: title, content: content, info: info, type: 'warning'});
    },

    arangoMessage: function (title, content, info) {
      window.App.notificationList.add({title: title, content: content, info: info, type: 'message'});
    },

    hideArangoNotifications: function () {
      $.noty.clearQueue();
      $.noty.closeAll();
    },

    openDocEditor: function (id, type, callback) {
      var ids = id.split('/');
      var self = this;

      var docFrameView = new window.DocumentView({
        collection: window.App.arangoDocumentStore
      });

      docFrameView.breadcrumb = function () {};

      docFrameView.colid = ids[0];
      docFrameView.docid = ids[1];

      docFrameView.el = '.arangoFrame .innerDiv';
      docFrameView.render();
      docFrameView.setType(type);

      /*
      if (docFrameView.collection.toJSON().length === 0) {
        this.closeDocEditor();
        return;
      }
      */

      // remove header
      $('.arangoFrame .headerBar').remove();
      // append close button
      $('.arangoFrame .outerDiv').prepend('<i class="fa fa-times"></i>');
      // add close events
      $('.arangoFrame .outerDiv').click(function () {
        self.closeDocEditor();
      });
      $('.arangoFrame .innerDiv').click(function (e) {
        e.stopPropagation();
      });
      $('.fa-times').click(function () {
        self.closeDocEditor();
      });

      $('.arangoFrame').show();

      docFrameView.customView = true;
      docFrameView.customDeleteFunction = function () {
        window.modalView.hide();
        $('.arangoFrame').hide();
      // callback()
      };

      $('.arangoFrame #deleteDocumentButton').click(function () {
        docFrameView.deleteDocumentModal();
      });
      $('.arangoFrame #saveDocumentButton').click(function () {
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
        type: 'POST',
        url: this.databaseUrl('/_admin/aardvark/job'),
        data: JSON.stringify(object),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          if (callback) {
            callback(false, data);
          }
        },
        error: function (data) {
          if (callback) {
            callback(true, data);
          }
        }
      });
    },

    deleteAardvarkJob: function (id, callback) {
      $.ajax({
        cache: false,
        type: 'DELETE',
        url: this.databaseUrl('/_admin/aardvark/job/' + encodeURIComponent(id)),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          if (callback) {
            callback(false, data);
          }
        },
        error: function (data) {
          if (callback) {
            callback(true, data);
          }
        }
      });
    },

    deleteAllAardvarkJobs: function (callback) {
      $.ajax({
        cache: false,
        type: 'DELETE',
        url: this.databaseUrl('/_admin/aardvark/job'),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          if (callback) {
            callback(false, data);
          }
        },
        error: function (data) {
          if (callback) {
            callback(true, data);
          }
        }
      });
    },

    getAardvarkJobs: function (callback) {
      $.ajax({
        cache: false,
        type: 'GET',
        url: this.databaseUrl('/_admin/aardvark/job'),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          if (callback) {
            callback(false, data);
          }
        },
        error: function (data) {
          if (callback) {
            callback(true, data);
          }
        }
      });
    },

    getPendingJobs: function (callback) {
      $.ajax({
        cache: false,
        type: 'GET',
        url: this.databaseUrl('/_api/job/pending'),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          callback(false, data);
        },
        error: function (data) {
          callback(true, data);
        }
      });
    },

    syncAndReturnUninishedAardvarkJobs: function (type, callback) {
      var callbackInner = function (error, AaJobs) {
        if (error) {
          callback(true);
        } else {
          var callbackInner2 = function (error, pendingJobs) {
            if (error) {
              arangoHelper.arangoError('', '');
            } else {
              var array = [];
              if (pendingJobs.length > 0) {
                _.each(AaJobs, function (aardvark) {
                  if (aardvark.type === type || aardvark.type === undefined) {
                    var found = false;
                    _.each(pendingJobs, function (pending) {
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
                    } else {
                      window.arangoHelper.deleteAardvarkJob(aardvark.id);
                    }
                  }
                });
              } else {
                if (AaJobs.length > 0) {
                  this.deleteAllAardvarkJobs();
                }
              }
              callback(false, array);
            }
          }.bind(this);

          this.getPendingJobs(callbackInner2);
        }
      }.bind(this);

      this.getAardvarkJobs(callbackInner);
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

    setDocumentStore: function (a) {
      this.arangoDocumentStore = a;
    },

    collectionApiType: function (identifier, refresh, toRun) {
      // set "refresh" to disable caching collection type
      if (refresh || this.CollectionTypes[identifier] === undefined) {
        var callback = function (error, data, toRun) {
          if (error) {
            arangoHelper.arangoError('Error', 'Could not detect collection type');
          } else {
            this.CollectionTypes[identifier] = data.type;
            if (this.CollectionTypes[identifier] === 3) {
              toRun(false, 'edge');
            } else {
              toRun(false, 'document');
            }
          }
        }.bind(this);
        this.arangoDocumentStore.getCollectionInfo(identifier, callback, toRun);
      } else {
        toRun(false, this.CollectionTypes[identifier]);
      }
    },

    collectionType: function (val) {
      if (!val || val.name === '') {
        return '-';
      }
      var type;
      if (val.type === 2) {
        type = 'document';
      } else if (val.type === 3) {
        type = 'edge';
      } else {
        type = 'unknown';
      }

      if (this.isSystemCollection(val)) {
        type += ' (system)';
      }

      return type;
    },

    formatDT: function (dt) {
      var pad = function (n) {
        return n < 10 ? '0' + n : n;
      };

      return dt.getUTCFullYear() + '-' +
        pad(dt.getUTCMonth() + 1) + '-' +
        pad(dt.getUTCDate()) + ' ' +
        pad(dt.getUTCHours()) + ':' +
        pad(dt.getUTCMinutes()) + ':' +
        pad(dt.getUTCSeconds());
    },

    escapeHtml: function (val) {
      // HTML-escape a string
      return String(val).replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
    },

    backendUrl: function (url) {
      return frontendConfig.basePath + url;
    },

    databaseUrl: function (url, databaseName) {
      if (url.substr(0, 5) === '/_db/') {
        throw new Error('Calling databaseUrl with a databased url (' + url + ") doesn't make any sense");
      }

      if (!databaseName) {
        databaseName = '_system';
        if (frontendConfig.db) {
          databaseName = frontendConfig.db;
        }
      }
      return this.backendUrl('/_db/' + encodeURIComponent(databaseName) + url);
    },

    showAuthDialog: function () {
      var toShow = true;
      var show = localStorage.getItem('authenticationNotification');

      if (show === 'false') {
        toShow = false;
      }

      return toShow;
    },

    doNotShowAgain: function () {
      localStorage.setItem('authenticationNotification', false);
    },

    renderEmpty: function (string, iconClass) {
      if (!iconClass) {
        $('#content').html(
          '<div class="noContent"><p>' + string + '</p></div>'
        );
      } else {
        $('#content').html(
          '<div class="noContent"><p>' + string + '<i class="' + iconClass + '"></i></p></div>'
        );
      }
    },

    initSigma: function () {
      // init sigma
      try {
        sigma.classes.graph.addMethod('neighbors', function (nodeId) {
          var k;
          var neighbors = {};
          var index = this.allNeighborsIndex[nodeId] || {};

          for (k in index) {
            neighbors[k] = this.nodesIndex[k];
          }
          return neighbors;
        });

        sigma.classes.graph.addMethod('getNodeEdges', function (nodeId) {
          var edges = this.edges();
          var edgesToReturn = [];

          _.each(edges, function (edge) {
            if (edge.source === nodeId || edge.target === nodeId) {
              edgesToReturn.push(edge.id);
            }
          });
          return edgesToReturn;
        });

        sigma.classes.graph.addMethod('getNodeEdgesCount', function (id) {
          return this.allNeighborsCount[id];
        });

        sigma.classes.graph.addMethod('getNodesCount', function () {
          return this.nodesArray.length;
        });
      } catch (ignore) {}
    },

    downloadLocalBlob: function (obj, type) {
      var dlType;
      if (type === 'csv') {
        dlType = 'text/csv; charset=utf-8';
      }

      if (dlType) {
        var blob = new Blob([obj], {type: dlType});
        var blobUrl = window.URL.createObjectURL(blob);
        var a = document.createElement('a');
        document.body.appendChild(a);
        a.style = 'display: none';
        a.href = blobUrl;
        a.download = 'results-' + window.frontendConfig.db + '.' + type;
        a.click();

        window.setTimeout(function () {
          window.URL.revokeObjectURL(blobUrl);
          document.body.removeChild(a);
        }, 500);
      }
    },

    download: function (url, callback) {
      $.ajax(url)
        .success(function (result, dummy, request) {
          if (callback) {
            callback(result);
            return;
          }

          var blob = new Blob([JSON.stringify(result)], {type: request.getResponseHeader('Content-Type') || 'application/octet-stream'});
          var blobUrl = window.URL.createObjectURL(blob);
          var a = document.createElement('a');
          document.body.appendChild(a);
          a.style = 'display: none';
          a.href = blobUrl;
          a.download = request.getResponseHeader('Content-Disposition').replace(/.* filename="([^")]*)"/, '$1');
          a.click();

          window.setTimeout(function () {
            window.URL.revokeObjectURL(blobUrl);
            document.body.removeChild(a);
          }, 500);
        });
    },

    checkCollectionPermissions: function (collectionID, roCallback) {
      var url = arangoHelper.databaseUrl('/_api/user/' +
        encodeURIComponent(window.App.userCollection.activeUser) +
        '/database/' + encodeURIComponent(frontendConfig.db) + '/' + encodeURIComponent(collectionID));

      // FETCH COMPLETE DB LIST
      $.ajax({
        type: 'GET',
        url: url,
        contentType: 'application/json',
        success: function (data) {
          // fetching available dbs and permissions
          if (data.result === 'ro') {
            roCallback();
          }
        },
        error: function (data) {
          arangoHelper.arangoError('User', 'Could not fetch collection permissions.');
        }
      });
    },

    checkDatabasePermissions: function (roCallback) {
      var url = arangoHelper.databaseUrl('/_api/user/' +
        encodeURIComponent(window.App.userCollection.activeUser) +
        '/database/' + encodeURIComponent(frontendConfig.db));

      // FETCH COMPLETE DB LIST
      $.ajax({
        type: 'GET',
        url: url,
        contentType: 'application/json',
        success: function (data) {
          // fetching available dbs and permissions
          if (data.result === 'ro') {
            roCallback();
          }
        },
        error: function (data) {
          arangoHelper.arangoError('User', 'Could not fetch collection permissions.');
        }
      });
    }

  };
}());
