/* global window, arangoHelper */
(function () {
  'use strict';

  window.ClusterServers = window.AutomaticRetryCollection.extend({
    model: window.ClusterServer,
    host: '',

    url: arangoHelper.databaseUrl('/_admin/aardvark/cluster/DBServers'),

    updateUrl: function () {
      // this.url = window.App.getNewRoute("DBServers")
      this.url = window.App.getNewRoute(this.host) + this.url;
    },

    initialize: function (models, options) {
      this.host = options.host;
    // window.App.registerForUpdate(this)
    },

    statusClass: function (s) {
      switch (s) {
        case 'ok':
          return 'success';
        case 'warning':
          return 'warning';
        case 'critical':
          return 'danger';
        case 'missing':
          return 'inactive';
        default:
          return 'danger';
      }
    },

    getStatuses: function (cb) {
      if (!this.checkRetries()) {
        return;
      }
      var self = this;
      var completed = function () {
        self.successFullTry();
        self._retryCount = 0;
        self.forEach(function (m) {
          cb(self.statusClass(m.get('status')), m.get('address'));
        });
      };
      // This is the first function called in
      // Each update loop
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.getStatuses.bind(self, cb))
      }).done(completed);
    },

    byAddress: function (res, callback) {
      if (!this.checkRetries()) {
        return;
      }
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.byAddress.bind(self, res, callback))
      }).done(function () {
        self.successFullTry();
        res = res || {};
        self.forEach(function (m) {
          var addr = m.get('address');
          addr = addr.split(':')[0];
          res[addr] = res[addr] || {};
          res[addr].dbs = res[addr].dbs || [];
          res[addr].dbs.push(m);
        });
        callback(res);
      }).error(function (e) {
        console.log('error');
        console.log(e);
      });
    },

    getList: function () {
      throw new Error('Do not use');
    },

    getOverview: function () {
      throw new Error('Do not use DbServer.getOverview');
    }
  });
}());
