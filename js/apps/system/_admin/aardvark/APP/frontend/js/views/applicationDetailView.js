/*jshint browser: true */
/*jshint strict: false, unused: false */
/*global Backbone, $, window, arangoHelper, templateEngine, Joi, _, modalDialogHelper*/

window.ApplicationDetailView = Backbone.View.extend({
  el: '#content',

  template: templateEngine.createTemplate('applicationDetailView.ejs'),

  events: {
    'click .open': 'openApp',
    'click .delete': 'deleteApp'
  },

  render: function() {
    $(this.el).html(this.template.render({
      app: this.model,
      db: arangoHelper.currentDatabase()
    }));

    if (!this.model.get('development') && !this.model.get('isSystem')) {
      $('.delete', this.el).prop('disabled', false);
    }

    $.get(this.appUrl()).success(function () {
      $('.open', this.el).prop('disabled', false);
    }, this);

    return $(this.el);
  },

  openApp: function() {
    window.open(this.appUrl(), this.model.get('title')).focus();
  },

  deleteApp: function() {
    var buttons = [];
    buttons.push(
      window.modalView.createDeleteButton("Delete", function() {
        this.model.destroy(function (result) {
          if (result.error === false) {
            window.modalView.hide();
            window.App.navigate("applications", { trigger: true });
          }
        });
      }.bind(this))
    );
    window.modalView.show(
      "modalDeleteConfirmation.ejs",
      "Delete Foxx App mounted at '" + this.model.get('mount') + "'",
      buttons,
      undefined,
      undefined,
      undefined,
      true
    );
  },

  appUrl: function () {
    return window.location.origin + '/_db/' +
      encodeURIComponent(arangoHelper.currentDatabase()) +
      this.model.get('mount');
  },
});
