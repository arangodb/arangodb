/*jshint browser: true */
/*jshint unused: false */
/*global window, document, Backbone, EJS, SwaggerUi, hljs, $, arangoHelper, templateEngine, Joi*/
(function() {

  "use strict";

  window.databaseView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate("databaseView.ejs"),

    dropdownVisible: false,

    currentDB: "",

    events: {
      "click #createDatabase"       : "createDatabase",
      "click #submitCreateDatabase" : "submitCreateDatabase",
      "click .editDatabase"         : "editDatabase",
      "click .icon"                 : "editDatabase",
      "click #selectDatabase"       : "updateDatabase",
      "click #submitDeleteDatabase" : "submitDeleteDatabase",
      "click .contentRowInactive a" : "changeDatabase",
      "keyup #databaseSearchInput"  : "search",
      "click #databaseSearchSubmit" : "search",
      "click #databaseToggle"       : "toggleSettingsDropdown",
      "click .css-label"            : "checkBoxes",
      "change #dbSortDesc"          : "sorting",
      "click svg"                   : "switchDatabase"
    },

    sorting: function() {
      if ($('#dbSortDesc').is(":checked")) {
        this.collection.setSortingDesc(true);
      }
      else {
        this.collection.setSortingDesc(false);
      }

      if ($('#databaseDropdown').is(":visible")) {
        this.dropdownVisible = true;
      } else {
        this.dropdownVisible = false;
      }

      this.render();
    },

    initialize: function() {
      this.collection.fetch({async:false});
    },

    checkBoxes: function (e) {
      //chrome bugfix
      var clicked = e.currentTarget.id;
      $('#'+clicked).click();
    },

    render: function(){
      this.currentDatabase();

      //sorting
      this.collection.sort();

      $(this.el).html(this.template.render({
        collection   : this.collection,
        searchString : '',
        currentDB    : this.currentDB
      }));

      if (this.dropdownVisible === true) {
        $('#dbSortDesc').attr('checked', this.collection.sortOptions.desc);
        $('#databaseToggle').toggleClass('activated');
        $('#databaseDropdown2').show();
      }

      this.replaceSVGs();
      return this;
    },

    toggleSettingsDropdown: function() {
      //apply sorting to checkboxes
      $('#dbSortDesc').attr('checked', this.collection.sortOptions.desc);

      $('#databaseToggle').toggleClass('activated');
      $('#databaseDropdown2').slideToggle(200);
    },

    selectedDatabase: function () {
      return $('#selectDatabases').val();
    },

    handleError: function(status, text, dbname) {
      if (status === 409) {
        arangoHelper.arangoError("DB", "Database " + dbname + " already exists.");
        return;
      }
      if (status === 400) {
        arangoHelper.arangoError("DB", "Invalid Parameters");
        return;
      }
      if (status === 403) {
        arangoHelper.arangoError("DB", "Insufficent rights. Execute this from _system database");
        return;
      }
    },

    validateDatabaseInfo: function (db, user, pw) {
      if (user === "") {
        arangoHelper.arangoError("DB", "You have to define an owner for the new database");
        return false;
      }
      if (db === "") {
        arangoHelper.arangoError("DB", "You have to define a name for the new database");
        return false;
      }
      if (db.indexOf("_") === 0) {
        arangoHelper.arangoError("DB ", "Databasename should not start with _");
        return false;
      }
      if (!db.match(/^[a-zA-Z][a-zA-Z0-9_\-]*$/)) {
        arangoHelper.arangoError("DB", "Databasename may only contain numbers, letters, _ and -");
        return false;
      }
      return true;
    },

    createDatabase: function(e) {
      e.preventDefault();
      this.createAddDatabaseModal();
    },

    switchDatabase: function(e) {
      var changeTo = $(e.currentTarget).parent().find("h5").text();
      var url = this.collection.createDatabaseURL(changeTo);
      window.location.replace(url);
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
          self.updateDatabases();
          window.modalView.hide();
          window.App.naviView.dbSelectionView.collection.fetch({
            success: function() {
              window.App.naviView.dbSelectionView.render($("#dbSelect"));
            }
          });
        }
      });
    },

    submitDeleteDatabase: function(dbname) {
      var toDelete = this.collection.where({name: dbname});
      toDelete[0].destroy({wait: true, url:"/_api/database/"+dbname});
      this.updateDatabases();
      window.App.naviView.dbSelectionView.collection.fetch({
        success: function() {
          window.App.naviView.dbSelectionView.render($("#dbSelect"));
        }
      });
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
        searchString : searchString,
        currentDB    : this.currentDB
      }));
      this.replaceSVGs();

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
        window.modalView.createReadOnlyEntry("id_name", "Name", dbName, "")
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
          true,
          [
            {
              rule: Joi.string().regex(/^[a-zA-Z]/),
              msg: "Database name must start with a letter."
            },
            {
              rule: Joi.string().regex(/^[a-zA-Z0-9\-_]*$/),
              msg: 'Only Symbols "_" and "-" are allowed.'
            },
            {
              rule: Joi.string().required(),
              msg: "No database name given."
            }
          ]
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
          true,
          [
            {
              rule: Joi.string().required(),
              msg: "No username given."
            }
          ]
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
