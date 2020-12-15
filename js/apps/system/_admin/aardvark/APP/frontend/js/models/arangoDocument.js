/* global window, Backbone, arangoHelper, _ */

window.arangoDocumentModel = Backbone.Model.extend({
  initialize: function () {
    'use strict';
  },
  urlRoot: arangoHelper.databaseUrl('/_api/document'),
  defaults: {
    _id: '',
    _rev: '',
    _key: ''
  },
  getSorted: function () {
    'use strict';
    var self = this;
    var keys = Object.keys(self.attributes).sort(function (l, r) {
      var l1 = arangoHelper.isSystemAttribute(l);
      var r1 = arangoHelper.isSystemAttribute(r);

      if (l1 !== r1) {
        if (l1) {
          return -1;
        }
        return 1;
      }

      return l < r ? -1 : 1;
    });

    var sorted = {};
    _.each(keys, function (k) {
      sorted[k] = self.attributes[k];
    });
    return sorted;
  }
});
