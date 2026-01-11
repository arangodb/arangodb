(function () {
  'use strict';
  window.CoordinatorCollection = Backbone.Collection.extend({
    model: window.Coordinator,

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
    }

  });
}());
