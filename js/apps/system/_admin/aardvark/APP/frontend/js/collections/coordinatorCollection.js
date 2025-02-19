(function () {
  'use strict';
  window.CoordinatorCollection = Backbone.Collection.extend({
    model: window.Coordinator,

    url: arangoHelper.databaseUrl('/_admin/aardvark/cluster/Coordinators')

  });
}());
