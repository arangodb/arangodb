(function () {
  'use strict';
  window.ClusterCoordinators = window.AutomaticRetryCollection.extend({
    model: window.ClusterCoordinator,

    // No longer using HTTP endpoint - data comes from cached health API result
    updateUrl: function () {
      // No-op - kept for compatibility
    },

    initialize: function () {
    },

    // Override fetch to use cached health data from window.App.lastHealthCheckResult
    fetch: function (options) {
      var self = this;
      options = options || {};

      var processHealth = function () {
        var healthData = window.App && window.App.lastHealthCheckResult
          ? window.App.lastHealthCheckResult.Health
          : null;

        if (!healthData) {
          if (options.error) {
            options.error.call(self);
          }
          return $.Deferred().reject().promise();
        }

        var models = arangoHelper.parseHealthToClusterModels(healthData, 'Coordinator');
        self.reset(models);
        self.successFullTry();

        if (options.success) {
          options.success.call(self, self, models, options);
        }

        return $.Deferred().resolve(self).promise();
      };

      // If health data is already available, use it immediately
      if (window.App && window.App.lastHealthCheckResult) {
        return processHealth();
      }

      // Otherwise wait for health data to become available
      var deferred = $.Deferred();
      var attempts = 0;
      var maxAttempts = 50; // 5 seconds max wait

      var waitForHealth = function () {
        if (window.App && window.App.lastHealthCheckResult) {
          processHealth();
          deferred.resolve(self);
        } else if (attempts++ < maxAttempts) {
          window.setTimeout(waitForHealth, 100);
        } else {
          if (options.error) {
            options.error.call(self);
          }
          deferred.reject();
        }
      };

      waitForHealth();
      return deferred.promise();
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
