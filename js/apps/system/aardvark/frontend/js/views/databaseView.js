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

    createDatabase: function(e) {
      e.preventDefault();
      this.createAddDatabaseModal();
    },

    submitCreateDatabase: function() {
      var self = this;
      var name  = $('#newDatabaseName').val();
      var userName = $('#newUser').val();
      var userPassword = $('#newPassword').val();
      if (!this.validateDatabaseInfo(name, userName, userPassword)) {
        console.log("VALIDATE");
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
          self.updateDatabases();
          window.modalView.hide();
        }
      });
    },

    submitDeleteDatabase: function(dbname) {
      var toDelete = this.collection.where({name: dbname});
      toDelete[0].destroy({wait: true, url:"/_api/database/"+dbname});
      this.updateDatabases();
      window.modalView.hide();
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
      var dbName = this.evaluateDatabaseName($(e.currentTarget).attr("id"), '_edit-database'),
        isDeleteable = true;
      if(dbName === this.currentDB) {
        isDeleteable = false;
      }
      this.createEditDatabaseModal(dbName, isDeleteable);
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
    },

    createEditDatabaseModal: function(dbName, isDeleteable) {
      var buttons = [],
        tableContent = [];

      tableContent.push(
        window.modalView.createReadOnlyEntry("Name", dbName, "")
      );
      if (isDeleteable) {
        buttons.push(
          window.modalView.createDeleteButton(
            "Delete",
            this.submitDeleteDatabase.bind(this, dbName)
          )
        );
      } else {
        buttons.push(window.modalView.createDisabledButton("Delete"));
      }
      window.modalView.show(
        "modalTable.ejs",
        "Edit database",
        buttons,
        tableContent
      );
    },

    createAddDatabaseModal: function() {
      var buttons = [],
        tableContent = [];

      tableContent.push(
        window.modalView.createTextEntry(
          "newDatabaseName",
          "Name",
          "",
          false,
          "Database Name",
          true
        )
      );
      tableContent.push(
        window.modalView.createTextEntry(
          "newUser",
          "Username",
          "",
          "Please define the owner of this database. This will be the only user having "
            + "initial access to this database. If the user is different to your account "
            + "you will not be able to see the database. "
            + "If there is a failure you will be informed.",
          "Database Owner",
          true
        )
      );
      tableContent.push(
        window.modalView.createPasswordEntry(
          "newPassword",
          "Password",
          "",
          false,
          "",
          false
        )
      );
      buttons.push(
        window.modalView.createSuccessButton(
          "Create",
          this.submitCreateDatabase.bind(this)
        )
      );
      window.modalView.show(
        "modalTable.ejs",
        "Create Database",
        buttons,
        tableContent
      );
    }



  });
}());
