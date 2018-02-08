/* jshint browser: true */
/* jshint unused: false */
/* global window, Backbone, $, arangoHelper */
(function () {
  'use strict';
  window.FoxxCollection = Backbone.Collection.extend({
    model: window.Foxx,

    sortOptions: {
      desc: false
    },

    url: arangoHelper.databaseUrl('/_admin/aardvark/foxxes'),

    comparator: function (item, item2) {
      var a, b;
      if (this.sortOptions.desc === true) {
        a = item.get('mount');
        b = item2.get('mount');
        return a < b ? 1 : a > b ? -1 : 0;
      }
      a = item.get('mount');
      b = item2.get('mount');
      return a > b ? 1 : a < b ? -1 : 0;
    },

    setSortingDesc: function (val) {
      this.sortOptions.desc = val;
    },

    // Install Foxx from github repo
    // info is expected to contain: "url" and "version"
    installFromGithub: function (info, mount, callback, isLegacy, flag) {
      var url = arangoHelper.databaseUrl('/_admin/aardvark/foxxes/git?mount=' + encodeURIComponent(mount));
      if (isLegacy) {
        url += '&legacy=true';
      }
      if (flag !== undefined) {
        if (flag) {
          url += '&replace=true';
        } else {
          url += '&upgrade=true';
        }
      }
      $.ajax({
        cache: false,
        type: 'PUT',
        url: url,
        data: JSON.stringify(info),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          callback(data);
        },
        error: function (err) {
          callback(err);
        }
      });
    },

    // Install Foxx from public url
    // info is expected to contain: "url" and "version"
    installFromUrl: function (info, mount, callback, isLegacy, flag) {
      var url = arangoHelper.databaseUrl('/_admin/aardvark/foxxes/url?mount=' + encodeURIComponent(mount));
      if (isLegacy) {
        url += '&legacy=true';
      }
      if (flag !== undefined) {
        if (flag) {
          url += '&replace=true';
        } else {
          url += '&upgrade=true';
        }
      }
      $.ajax({
        cache: false,
        type: 'PUT',
        url: url,
        data: JSON.stringify(info),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          callback(data);
        },
        error: function (err) {
          callback(err);
        }
      });
    },

    // Install Foxx from arango store
    // info is expected to contain: "name" and "version"
    installFromStore: function (info, mount, callback, flag) {
      var url = arangoHelper.databaseUrl('/_admin/aardvark/foxxes/store?mount=' + encodeURIComponent(mount));
      if (flag !== undefined) {
        if (flag) {
          url += '&replace=true';
        } else {
          url += '&upgrade=true';
        }
      }
      $.ajax({
        cache: false,
        type: 'PUT',
        url: url,
        data: JSON.stringify(info),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          callback(data);
        },
        error: function (err) {
          callback(err);
        }
      });
    },

    installFromZip: function (fileName, mount, callback, isLegacy, flag) {
      var url = arangoHelper.databaseUrl('/_admin/aardvark/foxxes/zip?mount=' + encodeURIComponent(mount));
      if (isLegacy) {
        url += '&legacy=true';
      }
      if (flag !== undefined) {
        if (flag) {
          url += '&replace=true';
        } else {
          url += '&upgrade=true';
        }
      }
      $.ajax({
        cache: false,
        type: 'PUT',
        url: url,
        data: JSON.stringify({zipFile: fileName}),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          callback(data);
        },
        error: function (err) {
          callback(err);
        }
      });
    },

    generate: function (info, mount, callback, flag) {
      var url = arangoHelper.databaseUrl('/_admin/aardvark/foxxes/generate?mount=' + encodeURIComponent(mount));
      if (flag !== undefined) {
        if (flag) {
          url += '&replace=true';
        } else {
          url += '&upgrade=true';
        }
      }
      $.ajax({
        cache: false,
        type: 'PUT',
        url: url,
        data: JSON.stringify(info),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          callback(data);
        },
        error: function (err) {
          callback(err);
        }
      });
    }

  });
}());
