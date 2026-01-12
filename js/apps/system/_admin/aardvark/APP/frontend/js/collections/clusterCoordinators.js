(function () {
  'use strict';
  window.ClusterCoordinators = window.AutomaticRetryCollection.extend({
    model: window.ClusterCoordinator,

    updateUrl: function () {
      // No-op - kept for compatibility
    },

    initialize: function () {
    },

    fetch: function (options) {
      var self = this;
      options = options || {};

      return arangoHelper.getHealthModels('Coordinator').done(function (models) {
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

    getStatuses: function (cb, nextStep) {
      if (!this.checkRetries()) {
        return;
      }
      var self = this;
      this.fetch({
        error: self.failureTry.bind(self, self.getStatuses.bind(self, cb, nextStep))
      }).done(function () {
        self.successFullTry();
        self.forEach(function (m) {
          cb(self.statusClass(m.get('status')), m.get('address'));
        });
        nextStep();
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
          res[addr].coords = res[addr].coords || [];
          res[addr].coords.push(m);
        });
        callback(res);
      });
    },

    checkConnection: function (callback) {
      var self = this;
      if (!this.checkRetries()) {
        return;
      }
      this.fetch({
        error: self.failureTry.bind(self, self.checkConnection.bind(self, callback))
      }).done(function () {
        self.successFullTry();
        callback();
      });
    }

  });
}());
