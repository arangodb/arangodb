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
    modalDialogHelper.createModalDeleteDialog(
      "Delete Foxx App mounted at '" + this.model.get('mount') + "'",
      "deleteFoxxApp",
      this.model,
      function(model) {
        model.destroy({
          success: function () {
            $("#deleteFoxxAppmodal").modal('hide');
            window.App.navigate("applications", { trigger: true });
          }
        });
      }
    );
  },

  appUrl: function () {
    return window.location.origin + '/_db/' +
      encodeURIComponent(arangoHelper.currentDatabase()) +
      this.model.get('mount');
  },
});
