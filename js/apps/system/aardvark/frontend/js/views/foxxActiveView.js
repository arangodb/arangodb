/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, $, window, EJS, arangoHelper, _, templateEngine, Joi*/

(function() {
  "use strict";
  var checkMount = function(mount) {
        var regex = /^(\/[^\/\s]+)+$/;
        if (!regex.test(mount)){
      arangoHelper.arangoError("Please give a valid mount point, e.g.: /myPath");
      return false;
    }
    return true;
  };

  window.FoxxActiveView = Backbone.View.extend({
    tagName: 'div',
    className: "tile",
    template: templateEngine.createTemplate("foxxActiveView.ejs"),

    events: {
      'click' : 'openAppDetailView'
    },

    initialize: function(){
      this._show = true;
      var mount = this.model.get("mount");
      var isSystem = (
        this.model.get("isSystem") &&
        mount.charAt(0) === '/' &&
          mount.charAt(1) === '_'
      );
      if (isSystem) {
        this.buttonConfig = [];
      } else {
        this.buttonConfig = [
          window.modalView.createDeleteButton(
            "Uninstall", this.uninstall.bind(this)
          ),
          window.modalView.createSuccessButton(
            "Save", this.changeFoxx.bind(this)
          )
        ];
      }
      this.showMod = window.modalView.show.bind(
        window.modalView,
        "modalTable.ejs",
        "Modify Application"
      );
      this.appsView = this.options.appsView;
      _.bindAll(this, 'render');
    },

    changeFoxx: function() {
      var mount = $("#change-mount-point").val();
      var failed = false;
      var app = this.model.get("app");
      var prefix = this.model.get("options").collectionPrefix;
      if (mount !== this.model.get("mount")) {
        if (checkMount(mount)) {
          $.ajax("foxx/move/" + this.model.get("_key"), {
            async: false,
            type: "PUT",
            data: JSON.stringify({
              mount: mount,
              app: app,
              prefix: prefix
            }),
            dataType: "json",
            error: function(data) {
              failed = true;
              arangoHelper.arangoError(data.responseText);
            }
          });
        } else {
          return;
        }
      }
      // TO_DO change version
      if (!failed) {
        window.modalView.hide();
        this.appsView.reload();
      }
    },

    openAppDetailView: function() {
      // var url = window.location.origin + "/_db/" +
      //           encodeURIComponent(arangoHelper.currentDatabase()) +
      //           this.model.get("mount");
      // var windowRef = window.open(url, this.model.get("title"));
      alert('Michael will insert something here for ' + this.model.get('mount') + '. Stay tuned.');
    },

    render: function(){
      if (this._show) {
        $(this.el).html(this.template.render(this.model));
      }

      return $(this.el);
    }
  });
}());
