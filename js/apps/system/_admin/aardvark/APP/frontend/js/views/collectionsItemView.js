/* jshint browser: true */
/* jshint unused: false */
/* global window, frontendConfig, Backbone, _, $, templateEngine, arangoHelper, Joi */

(function () {
  'use strict';

  window.CollectionListItemView = Backbone.View.extend({
    tagName: 'div',
    className: 'tile pure-u-1-1 pure-u-sm-1-2 pure-u-md-1-3 pure-u-lg-1-4 pure-u-xl-1-6',
    template: templateEngine.createTemplate('collectionsItemView.ejs'),

    initialize: function (options) {
      this.collectionsView = options.collectionsView;
    },

    events: {
      'click': 'selectCollection'
    },

    render: function () {
      if (this.model.get('locked') || this.model.get('status') === 'corrupted') {
        $(this.el).addClass('locked');
        $(this.el).addClass(this.model.get('lockType'));
      } else {
        $(this.el).removeClass('locked');
      }
      $(this.el).html(this.template.render({
        model: this.model
      }));
      $(this.el).attr('id', 'collection_' + this.model.get('name'));

      return this;
    },

    selectCollection: function (event) {
      // check if event was fired from disabled button
      if ($(event.target).hasClass('disabled')) {
        return 0;
      }
      if (this.model.get('locked')) {
        return 0;
      }
      if (this.model.get('status') === 'corrupted') {
        return 0;
      }

      window.App.navigate(
        'collection/' + encodeURIComponent(this.model.get('name')) + '/documents/1', {trigger: true}
      );
    },

  });
}());
