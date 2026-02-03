(function () {
  'use strict';
  window.CoordinatorCollection = Backbone.Collection.extend({
    model: window.Coordinator,

    fetch: function (options) {
      var self = this;
      options = options || {};

      return arangoHelper.getHealthModels('Coordinator').done(function (models) {
        self.reset(models);
        if (options.success) {
          options.success.call(self, self, models, options);
        }
      }).fail(function () {
        if (options.error) {
          options.error.call(self);
        }
      });
    }

  });
}());
