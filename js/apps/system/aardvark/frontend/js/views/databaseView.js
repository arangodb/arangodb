/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global window, document, Backbone, EJS, SwaggerUi, hljs, $ */

window.databaseView = Backbone.View.extend({
  el: '#content',

  template: new EJS({url: 'js/templates/databaseView.ejs'}),

  currentDB: "",

  events: {
    "click #createDatabase"       : "createDatabase",
    "click #submitCreateDatabase" : "submitCreateDatabase",
    "click #selectDatabase"       : "updateDatabase",
    "click #databaseTable .glyphicon-minus-sign" : "removeDatabase",
    "click #submitDeleteDatabase" : "submitRemoveDatabase"
  },

  initialize: function() {
    var self = this;
    this.collection.fetch();
  },

  render: function(){
    $(this.el).html(this.template.render({}));
    this.renderTable();
    return this;
  },

  renderTable: function () {
    this.collection.map(function(dbs) {
      $("#databaseTable tbody").append(
        '<tr><td>' + dbs.get("name") + '</td>' +
        '<td><span class="glyphicon glyphicon-minus-sign"></span></td></tr>'
      );
    });
  },

  selectedDatabase: function () {
    return $('#selectDatabases').val();
  },

  submitCreateDatabase: function() {
    this.collection.create({name: $('#newDatabaseName').val()}, {wait:true});
    this.hideModal();
    this.updateDatabases();
  },

  hideModal: function() {
    $('#createDatabaseModal').modal('hide');
  },

  showModal: function() {
    $('#createDatabaseModal').modal('show');
  },

  createDatabase: function() {
    this.showModal();
  },

  submitRemoveDatabase: function(e) {
    var toDelete = this.collection.where({name: this.dbToDelete});
    toDelete[0].destroy({wait: true, url:"/_api/database/"+this.dbToDelete});
    this.dbToDelete = '';
    $('#deleteDatabaseModal').modal('hide');
    this.updateDatabases();
  },

  removeDatabase: function(e) {
    this.dbToDelete = $(e.currentTarget).parent().parent().children().first().text();
    $('#deleteDatabaseModal').modal('show');
  },

  selectDatabase: function() {
    this.currentDB = this.collection.getCurrentDatabase();

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
