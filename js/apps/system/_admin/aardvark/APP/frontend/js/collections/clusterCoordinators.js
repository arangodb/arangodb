/* global window, arangoHelper */
(function () {
  'use strict';
  window.ClusterCoordinators = window.AutomaticRetryCollection.extend({
    model: window.ClusterCoordinator,

    url: arangoHelper.databaseUrl('/_admin/aardvark/cluster/Coordinators'),

    updateUrl: function () {
      this.url = window.App.getNewRoute('Coordinators');
    },

    initialize: function () {
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

    getStatuses: function (cb, nextStep) {
      if (!this.checkRetries()) {
        return;
      }
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
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
        beforeSend: window.App.addAuth.bind(window.App),
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
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.checkConnection.bind(self, callback))
      }).done(function () {
        self.successFullTry();
        callback();
      });
    }

  });
}());
