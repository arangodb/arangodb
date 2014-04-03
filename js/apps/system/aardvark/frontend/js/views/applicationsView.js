/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global Backbone, EJS, $, window, arangoHelper, templateEngine, _, console */

window.ApplicationsView = Backbone.View.extend({
  el: '#content',

  template: templateEngine.createTemplate("applicationsView.ejs"),

  events: {
    "click #checkDevel"         : "toggleDevel",
    "click #checkActive"        : "toggleActive",
    "click #checkInactive"      : "toggleInactive",
    "click #foxxToggle"         : "slideToggle",
    "click #importFoxxToggle"   : "slideToggleImport",
    "change #importFoxx"        : "uploadSetup",
    "click #confirmFoxxImport"  : "importFoxx"
  },

  uploadSetup: function (e) {
    var files = e.target.files || e.dataTransfer.files;
    this.file = files[0];
    this.allowUpload = true;
  },

  importFoxx: function() {
    var self = this;
    if (this.allowUpload) {
      this.showSpinner();
      $.ajax({
        type: "POST",
        async: false,
        url: '/_api/upload',
        data: self.file,
        processData: false,
        contentType: 'application/octet-stream',
        complete: function(res) {
          if (res.readyState === 4) {
            if (res.status === 201) {
              $.ajax({
                type: "POST",
                async: false,
                url: "/_admin/aardvark/foxxes/inspect",
                data: res.responseText,
                contentType: "application/json"
              }).done(function(res) {
                console.log(res);
                $.ajax({
                  type: "POST",
                  async: false,
                  url: '/_admin/foxx/fetch',
                  data: JSON.stringify({ 
                    name: res.name,
                    version: res.version,
                    filename: res.filename
                  }),
                  processData: false
                }).done(function () {
                  self.reload();
                }).fail(function (err) {
                  self.hideSpinner();
                  var error = JSON.parse(err.responseText);
                  arangoHelper.arangoError("Error: " + error.errorMessage);
                });
              }).fail(function(err) {
                self.hideSpinner();
                var error = JSON.parse(err.responseText);
                arangoHelper.arangoError("Error: " + error.error);
              });
              delete self.file;
              self.allowUpload = false;
              self.hideSpinner();
              self.hideImportModal();
              return;
            }
          }
          self.hideSpinner();
          arangoHelper.arangoError("Upload error");
        }
      });
    }
  },
  
  showSpinner: function() {
    $('#uploadIndicator').show();
  },

  hideSpinner: function() {
    $('#uploadIndicator').hide();
  },

  toggleDevel: function() {
    var self = this;
    this._showDevel = !this._showDevel;
    _.each(this._installedSubViews, function(v) {
      v.toggle("devel", self._showDevel);
    });
  },
  
  toggleActive: function() {
    var self = this;
    this._showActive = !this._showActive;
    _.each(this._installedSubViews, function(v) {
      v.toggle("active", self._showActive);
    });
  },
  
  toggleInactive: function() {
    var self = this;
    this._showInactive = !this._showInactive;
    _.each(this._installedSubViews, function(v) {
      v.toggle("inactive", self._showInactive);
    });
  },

  hideImportModal: function() {
    $('#foxxDropdownImport').hide();
  },

  hideSettingsModal: function() {
    $('#foxxDropdownOut').hide();
  },

  slideToggleImport: function() {
    $('#foxxToggle').removeClass('activated');
    $('#importFoxxToggle').toggleClass('activated');
    $('#foxxDropdownImport').slideToggle(200);
    this.hideSettingsModal();
  },

  slideToggle: function() {
    $('#importFoxxToggle').removeClass('activated');
    $('#foxxToggle').toggleClass('activated');
    $('#foxxDropdownOut').slideToggle(200);
    this.hideImportModal();
  },

  reload: function() {
    var self = this;

    // unbind and remove any unused views
    _.each(this._installedSubViews, function (v) {
      v.undelegateEvents();
    });
    _.each(this._availableSubViews, function (v) {
      v.undelegateEvents();
    });

    this._installedSubViews = { };
    this._availableSubViews = { };

    this.collection.fetch({
      success: function() {
        self.collection.each(function (foxx) {
          var subView;
          if (foxx.get("type") === "app") {
            subView = new window.FoxxInstalledView({
              model: foxx,
              appsView: self
            });
            self._availableSubViews[foxx.get('_id')] = subView;
          } else if (foxx.get("type") === "mount") {
            subView = new window.FoxxActiveView({
              model: foxx,
              appsView: self
            });
            self._installedSubViews[foxx.get('_id')] = subView;
          }
        });
        self.render();
      }
    });
  },
  
  initialize: function() {    
    this._installedSubViews = {};
    this._availableSubViews = {};
    this._showDevel = true;
    this._showActive = true;
    this._showInactive = true;
    this.reload();
  },
  
  render: function() {
    $(this.el).html(this.template.render({}));
    var self = this;
    _.each(this._installedSubViews, function (v) {
      $("#installedList").append(v.render());
    });
    _.each(this._availableSubViews, function (v) {
      $("#availableList").append(v.render());
    });
    this.delegateEvents();
    $('#checkActive').attr('checked', this._showActive);
    $('#checkInactive').attr('checked', this._showInactive);
    $('#checkDevel').attr('checked', this._showDevel);
    _.each(this._installedSubViews, function(v) {
      v.toggle("devel", self._showDevel);
      v.toggle("active", self._showActive);
      v.toggle("inactive", self._showInactive);
    });
  
    arangoHelper.fixTooltips("icon_arangodb", "left");
    return this;
  }
});
