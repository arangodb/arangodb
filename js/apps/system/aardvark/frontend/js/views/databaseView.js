/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global window, document, Backbone, EJS, SwaggerUi, hljs, $ */

window.databaseView = Backbone.View.extend({
  el: '#content',

  template: new EJS({url: 'js/templates/databaseView.ejs'}),

  events: {
    "click #createDatabase" : "createDatabase",
    "click #removeDatabase" : "removeDatabase",
    "click #selectDatabase" : "updateDatabase"
  },

  initialize: function() {
    var self = this;
    this.collection.fetch();
  },

  render: function(){
    $(this.el).html(this.template.render({}));
    this.renderOptions();
    return this;
  },

  renderOptions: function () {
    this.collection.map(function(dbs) {
      $('#selectDatabases').append( new Option(dbs.get("name"),dbs.get("name")) );
    });
  },

  selectedDatabase: function () {
    return $('#selectDatabases').val();
  },

  createDatabase: function() {
    this.collection.create({name:"helloworld"});
  },

  removeDatabase: function() {
    var toDelete = this.collection.where({name: this.selectedDatabase()});
    console.log(toDelete[0]);
    toDelete[0].destroy({wait: true});
    this.updateDatabases();
  },

  selectDatabase: function() {

  },

  updateDatabases: function() {
    var self = this;
    this.collection.fetch({
      success: function() {
        self.render();
      }
    });
  }

});
