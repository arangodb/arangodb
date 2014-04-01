/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, document, Backbone, EJS, SwaggerUi, hljs, $, arangoHelper, templateEngine */
(function() {

  "use strict";

  window.databaseView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate("databaseView.ejs"),

    currentDB: "",

    events: {
      "click #createDatabase"       : "createDatabase",
      "click #submitCreateDatabase" : "submitCreateDatabase",
      "click .editDatabase"         : "editDatabase",
      "click .icon"                 : "editDatabase",
      "click #selectDatabase"       : "updateDatabase",
      "click #deleteDatabase"       : "deleteDatabase",
      "click #submitDeleteDatabase" : "submitDeleteDatabase",
      "click .contentRowInactive a" : "changeDatabase",
      "keyup #databaseSearchInput"  : "search",
      "click #databaseSearchSubmit" : "search"
    },

    initialize: function() {
      this.collection.fetch({async:false});
    },

    render: function(){
      this.currentDatabase();
      this.collection.sort();

      $(this.el).html(this.template.render({
        collection   : this.collection,
        searchString : '',
        currentDB    : this.currentDB
      }));
      this.replaceSVGs();
      return this;
    },

    selectedDatabase: function () {
      return $('#selectDatabases').val();
    },

    handleError: function(status, text, dbname) {
      if (status === 409) {
        arangoHelper.arangoError("Database " + dbname + " already exists.");
        return;
      }
      if (status === 400) {
        arangoHelper.arangoError("Invalid Parameters");
        return;
      }
      if (status === 403) {
        arangoHelper.arangoError("Insufficent rights. Execute this from _system database");
        return;
      }
    },

    validateDatabaseInfo: function (db, user, pw) {
      if (user === "") {
        arangoHelper.arangoError("You have to define an owner for the new database");
        return false;
      }
      if (db === "") {
        arangoHelper.arangoError("You have to define a name for the new database"); 
        return false;
      }
      if (db.indexOf("_") === 0) {
        arangoHelper.arangoError("Databasename should not start with _");
        return false;
      }
      if (!db.match(/^[a-zA-Z][a-zA-Z0-9_\-]*$/)) {
        arangoHelper.arangoError("Databasename may only contain numbers, letters, _ and -");
        return false;
      }
      return true;
    },

    submitCreateDatabase: function() {
      var self = this;
      var name  = $('#newDatabaseName').val();
      var userName = $('#newUser').val();
      var userPassword = $('#newPassword').val();
      if (!this.validateDatabaseInfo(name, userName, userPassword)) {
        return;
      }
      var options = {
        name: name,
        users: [
          {
            username: userName,
            passwd: userPassword,
            active: true
          }
        ]
      };
      this.collection.create(options, {
        wait:true,
        error: function(data, err) {
          self.handleError(err.status, err.statusText, name);
        },
        success: function(data) {
          self.hideModal('createDatabaseModal');
          self.updateDatabases();
        }
      });
    },

    hideModal: function(modal) {
      $('#' + modal).modal('hide');
    },

    showModal: function(modal) {
      $('#' + modal).modal('show');
    },

    createDatabase: function(e) {
      e.preventDefault();
      this.showModal('createDatabaseModal');
    },

    submitDeleteDatabase: function(e) {
      var toDelete = this.collection.where({name: this.dbToDelete});
      toDelete[0].destroy({wait: true, url:"/_api/database/"+this.dbToDelete});
      this.dbToDelete = '';
      this.hideModal('deleteDatabaseModal');
      this.updateDatabases();
    },

    deleteDatabase: function(e) {
      this.hideModal('editDatabaseModal');
      this.dbToDelete = $('#editDatabaseName').html();
      this.showModal('deleteDatabaseModal');
    },

    currentDatabase: function() {
      this.currentDB = this.collection.getCurrentDatabase();
    },

    changeDatabase: function(e) {
      var changeTo = $(e.currentTarget).attr("id");
      var url = this.collection.createDatabaseURL(changeTo);
      window.location.replace(url);
    },

    updateDatabases: function() {
      var self = this;
      this.collection.fetch({
        success: function() {
          self.render();
          window.App.handleSelectDatabase();
        }
      });
    },

    editDatabase: function(e) {
      var dbName = this.evaluateDatabaseName($(e.currentTarget).attr("id"), '_edit-database');
      $('#editDatabaseName').html(dbName);
      var button = $('#deleteDatabase');
      if(dbName === this.currentDB) {
        var element;
        button.prop('disabled', true);
        button.removeClass('button-danger');
        button.addClass('button-neutral');
      } else {
        button.prop('disabled', false);
        button.removeClass('button-neutral');
        button.addClass('button-danger');
      }
      this.showModal('editDatabaseModal');
    },

    search: function() {
      var searchInput,
        searchString,
        strLength,
        reducedCollection;

      searchInput = $('#databaseSearchInput');
      searchString = $("#databaseSearchInput").val();
      reducedCollection = this.collection.filter(
        function(u) {
          return u.get("name").indexOf(searchString) !== -1;
        }
      );
      $(this.el).html(this.template.render({
        collection   : reducedCollection,
        searchString : searchString
      }));

      //after rendering, get the "new" element
      searchInput = $('#databaseSearchInput');
      //set focus on end of text in input field
      strLength= searchInput.val().length;
      searchInput.focus();
      searchInput[0].setSelectionRange(strLength, strLength);
    },

    replaceSVGs: function() {
      $(".svgToReplace").each(function() {
        var img = $(this);
        var id = img.attr("id");
        var src = img.attr("src");
        $.get(src, function(d) {
          var svg = $(d).find("svg");
          svg.attr("id", id)
            .attr("class", "tile-icon-svg")
            .removeAttr("xmlns:a");
          img.replaceWith(svg);
        }, "xml");
      });
    },

    evaluateDatabaseName : function(str, substr) {
      var index = str.lastIndexOf(substr);
      return str.substring(0, index);
    }



  });
}());
