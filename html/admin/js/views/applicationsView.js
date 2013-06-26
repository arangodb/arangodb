/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global Backbone, EJS, $, window, _ */

var ApplicationsView = Backbone.View.extend({
  el: '#content',

  template: new EJS({url: 'js/templates/applicationsView.ejs'}),
  
  events: {
    "click .toggle-icon": "toggleView",
    "click #checkDevel": "toggleDevel",
    "click #checkActive": "toggleActive",
    "click #checkInactive": "toggleInactive"
  },
  
  toggleDevel: function() {
    var self = this;
    this._showDevel = !this._showDevel;
    _.each(this._installedSubViews, function(v) {
      v.toggle("devel", self._showDevel)
    });
  },
  
  toggleActive: function() {
    var self = this;
    this._showActive = !this._showActive;
    _.each(this._installedSubViews, function(v) {
      v.toggle("active", self._showActive)
    });
  },
  
  toggleInactive: function() {
    var self = this;
    this._showInactive = !this._showInactive;
    _.each(this._installedSubViews, function(v) {
      v.toggle("inactive", self._showInactive)
    });
  },
  
  toggleView: function(event) {
    var target = $(event.currentTarget);
    var type = target.attr("id");
    var close = target.hasClass("icon-minus");
    var selector = "";
    if (type === "toggleInstalled") {
      selector = "#installedList";
    } else if (type === "toggleAvailable") {
      selector = "#availableList";
    }
    if (close) {
      $(selector).hide();
    } else {
      $(selector).show();
    }
    target.toggleClass("icon-minus");
    target.toggleClass("icon-plus");
    event.stopPropagation();
  },
  
  reload: function() {
    var self = this;
    this.collection.fetch({
      success: function() {
        self.collection.each(function (foxx) {
          var subView;
          if (foxx.get("type") === "app") {
            subView = new window.FoxxInstalledView({model: foxx});
            self._availableSubViews[foxx.get('_id')] = subView;
          } else if (foxx.get("type") === "mount") {
            subView = new window.FoxxActiveView({model: foxx});
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
    $(this.el).html(this.template.text);
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
    return this;
  }
});
