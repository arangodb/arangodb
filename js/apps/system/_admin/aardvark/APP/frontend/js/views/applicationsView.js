/*jshint browser: true */
/*global Backbone, $, window, arangoHelper, templateEngine, _*/
(function() {
  "use strict";

  window.ApplicationsView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate("applicationsView.ejs"),

    events: {
      "click #addApp"                : "createInstallModal",
      "click #foxxToggle"            : "slideToggle",
      "click #checkDevel"            : "toggleDevel",
      "click #checkProduction"       : "toggleProduction",
      "click #checkSystem"           : "toggleSystem",
      "click .css-label"             : "checkBoxes"
    },


    checkBoxes: function (e) {
      e.preventDefault();
      //chrome bugfix
      var toClick = $(e.currentTarget).closest('.checkboxLabel');
      
      if (toClick.length > 0) {
        var other = $('input', toClick);
        other[0].click();
      }
    },

    toggleDevel: function(e) {
      e.preventDefault();
      this._showDevel = !this._showDevel;
      this.render();
    },

    toggleProduction: function(e) {
      e.preventDefault();
      this._showProd = !this._showProd;
      this.render();
    },

    toggleSystem: function(e) {
      e.preventDefault();
      this._showSystem = !this._showSystem;
      this.render();
    },

    reload: function() {
      var self = this;

      // unbind and remove any unused views
      _.each(this._installedSubViews, function (v) {
        v.undelegateEvents();
      });

      this.collection.fetch({
        success: function() {
          self.createSubViews();
          self.render();
        }
      });
    },

    createSubViews: function() {
      var self = this;
      this._installedSubViews = { };

      self.collection.each(function (foxx) {
        var subView = new window.FoxxActiveView({
          model: foxx,
          appsView: self
        });
        self._installedSubViews[foxx.get('mount')] = subView;
      });
    },

    initialize: function() {
      this._installedSubViews = {};
      this._showDevel = true;
      this._showProd = true;
      this._showSystem = false;
      this.reload();
    },

    slideToggle: function() {
      $('#foxxToggle').toggleClass('activated');
      $('#foxxDropdownOut').slideToggle(200);
    },

    createInstallModal: function(event) {
      event.preventDefault();
      window.foxxInstallView.install(this.reload.bind(this));
    },

    render: function() {
      var dropdownVisible = false;
      if ($('#foxxDropdown').is(':visible')) {
        dropdownVisible = true;
      }

      this.collection.sort();

      $(this.el).html(this.template.render({}));
      _.each(this._installedSubViews, function (v) {
        $("#installedList").append(v.render());
      });
      this.delegateEvents();
      $('#checkDevel').attr('checked', this._showDevel);
      $('#checkProduction').attr('checked', this._showProd);
      $('#checkSystem').attr('checked', this._showSystem);
      
      if (dropdownVisible === true) {
        $('#foxxDropdownOut').show();
      }
      
      var self = this;
      _.each(this._installedSubViews, function(v) {
        v.toggle("devel", self._showDevel);
        v.toggle("system", self._showSystem);
      });

      arangoHelper.fixTooltips("icon_arangodb", "left");
      return this;
    }

  });

  /* Info for mountpoint
   *
   *
      window.modalView.createTextEntry(
        "mount-point",
        "Mount",
        "",
        "The path the app will be mounted. Has to start with /. Is not allowed to start with /_",
        "/my/app",
        true,
        [
          {
            rule: Joi.string().required(),
            msg: "No mountpoint given."
          },
          {
            rule: Joi.string().regex(/^\/[^_]/),
            msg: "Mountpoints with _ are reserved for internal use."
          },
          {
            rule: Joi.string().regex(/^(\/[a-zA-Z0-9_\-%]+)+$/),
            msg: "Mountpoints have to start with / and can only contain [a-zA-Z0-9_-%]"
          }
        ]
      )
   */
}());
