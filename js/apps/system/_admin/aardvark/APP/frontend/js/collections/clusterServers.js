(function () {
  'use strict';

  window.ClusterServers = window.AutomaticRetryCollection.extend({
    model: window.ClusterServer,
    host: '',

    updateUrl: function () {
      // No-op - kept for compatibility
    },

    initialize: function (models, options) {
      this.host = options.host;
    },

    fetch: function (options) {
      var self = this;
      options = options || {};

      return arangoHelper.getHealthModels('DBServer').done(function (models) {
        self.reset(models);
        self.successFullTry();
        if (options.success) {
          options.success.call(self, self, models, options);
        }
      }).fail(function () {
        if (options.error) {
          options.error.call(self);
        }
      });
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
      this.fetch({
        error: self.failureTry.bind(self, self.getStatuses.bind(self, cb))
      }).done(function () {
        self.successFullTry();
        self._retryCount = 0;
        self.forEach(function (m) {
          cb(self.statusClass(m.get('status')), m.get('address'));
        });
      });
    },

    byAddress: function (res, callback) {
      if (!this.checkRetries()) {
        return;
      }
      var self = this;
      this.fetch({
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
