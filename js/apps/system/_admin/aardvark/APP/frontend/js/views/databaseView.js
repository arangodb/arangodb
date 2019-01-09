/* jshint browser: true */
/* jshint unused: false */
/* global window, document, Backbone, $, arangoHelper, templateEngine, Joi */
(function () {
  'use strict';

  window.DatabaseView = Backbone.View.extend({
    users: null,
    el: '#content',
    readOnly: false,

    template: templateEngine.createTemplate('databaseView.ejs'),

    dropdownVisible: false,

    currentDB: '',

    events: {
      'click #createDatabase': 'createDatabase',
      'click #submitCreateDatabase': 'submitCreateDatabase',
      'click .editDatabase': 'editDatabase',
      'click #userManagementView .icon': 'editDatabase',
      'click #selectDatabase': 'updateDatabase',
      'click #submitDeleteDatabase': 'submitDeleteDatabase',
      'click .contentRowInactive a': 'changeDatabase',
      'keyup #databaseSearchInput': 'search',
      'click #databaseSearchSubmit': 'search',
      'click #databaseToggle': 'toggleSettingsDropdown',
      'click .css-label': 'checkBoxes',
      'click #dbSortDesc': 'sorting'
    },

    sorting: function () {
      if ($('#dbSortDesc').is(':checked')) {
        this.collection.setSortingDesc(true);
      } else {
        this.collection.setSortingDesc(false);
      }

      this.checkVisibility();
      this.render();
    },

    initialize: function () {
      this.collection.fetch({
        async: true,
        cache: false
      });
    },

    checkBoxes: function (e) {
      // chrome bugfix
      var clicked = e.currentTarget.id;
      $('#' + clicked).click();
    },

    setReadOnly: function () {
      $('#createDatabase').parent().parent().addClass('disabled');
    },

    render: function () {
      arangoHelper.checkDatabasePermissions(this.continueRender.bind(this), this.continueRender.bind(this));
    },

    continueRender: function (readOnly) {
      var self = this;
      if (readOnly) {
        this.readOnly = readOnly;
      }

      var callback = function (error, db) {
        if (error) {
          arangoHelper.arangoError('DB', 'Could not get current db properties');
        } else {
          self.currentDB = db;

          self.collection.fetch({
            cache: false,
            success: function () {
              // sorting
              self.collection.sort();

              $(self.el).html(self.template.render({
                collection: self.collection,
                searchString: '',
                currentDB: self.currentDB,
                readOnly: readOnly
              }));

              if (readOnly) {
                self.setReadOnly();
              }

              if (self.dropdownVisible === true) {
                $('#dbSortDesc').attr('checked', self.collection.sortOptions.desc);
                $('#databaseToggle').addClass('activated');
                $('#databaseDropdown2').show();
              }

              arangoHelper.setCheckboxStatus('#databaseDropdown');

              self.replaceSVGs();
            }
          });
        }
      };

      this.collection.getCurrentDatabase(callback);

      return this;
    },

    checkVisibility: function () {
      if ($('#databaseDropdown').is(':visible')) {
        this.dropdownVisible = true;
      } else {
        this.dropdownVisible = false;
      }
      arangoHelper.setCheckboxStatus('#databaseDropdown');
    },

    toggleSettingsDropdown: function () {
      var self = this;
      // apply sorting to checkboxes
      $('#dbSortDesc').attr('checked', this.collection.sortOptions.desc);

      $('#databaseToggle').toggleClass('activated');
      $('#databaseDropdown2').slideToggle(200, function () {
        self.checkVisibility();
      });
    },

    selectedDatabase: function () {
      return $('#selectDatabases').val();
    },

    handleError: function (status, text, dbname) {
      if (status === 409) {
        arangoHelper.arangoError('DB', 'Database ' + dbname + ' already exists.');
      } else if (status === 400) {
        arangoHelper.arangoError('DB', 'Invalid Parameters');
      } else if (status === 403) {
        arangoHelper.arangoError('DB', 'Insufficent rights. Execute this from _system database');
      }
    },

    validateDatabaseInfo: function (db, user) {
      if (user === '') {
        arangoHelper.arangoError('DB', 'You have to define an owner for the new database');
        return false;
      }
      if (db === '') {
        arangoHelper.arangoError('DB', 'You have to define a name for the new database');
        return false;
      }
      if (db.indexOf('_') === 0) {
        arangoHelper.arangoError('DB ', 'Databasename should not start with _');
        return false;
      }
      if (!db.match(/^[a-zA-Z][a-zA-Z0-9_-]*$/)) {
        arangoHelper.arangoError('DB', 'Databasename may only contain numbers, letters, _ and -');
        return false;
      }
      return true;
    },

    createDatabase: function (e) {
      e.preventDefault();
      if (!$('#createDatabase').parent().parent().hasClass('disabled')) {
        this.createAddDatabaseModal();
      }
    },

    switchDatabase: function (e) {
      if (!$(e.target).parent().hasClass('iconSet')) {
        var changeTo = $(e.currentTarget).find('h5').text();
        if (changeTo !== '') {
          var url = this.collection.createDatabaseURL(changeTo);
          window.location.replace(url);
        }
      }
    },

    submitCreateDatabase: function () {
      var self = this; // userPassword,
      var dbname = $('#newDatabaseName').val();
      var userName = $('#newUser').val();

      var options = {
        name: dbname,
        users: [{
          username: userName
        }]
      };

      this.collection.create(options, {
        error: function (data, err) {
          self.handleError(err.status, err.statusText, dbname);
        },
        success: function (data) {
          if (window.location.hash === '#databases') {
            self.updateDatabases();
          }
          arangoHelper.arangoNotification('Database ' + data.get('name') + ' created.');
        }
      });

      arangoHelper.arangoNotification('Database creation in progress.');
      window.modalView.hide();
    },

    submitDeleteDatabase: function (dbname) {
      var toDelete = this.collection.where({name: dbname});
      toDelete[0].destroy({wait: true, url: arangoHelper.databaseUrl('/_api/database/' + dbname)});
      this.updateDatabases();
      window.App.naviView.dbSelectionView.render($('#dbSelect'));
      window.modalView.hide();
    },

    changeDatabase: function (e) {
      var changeTo = $(e.currentTarget).attr('id');
      var url = this.collection.createDatabaseURL(changeTo);
      window.location.replace(url);
    },

    updateDatabases: function () {
      var self = this;
      this.collection.fetch({
        cache: false,
        success: function () {
          self.render();
          window.App.handleSelectDatabase();
        }
      });
    },

    editDatabase: function (e) {
      var dbName = this.evaluateDatabaseName($(e.currentTarget).attr('id'), '_edit-database');
      var isDeletable = true;
      if (dbName === this.currentDB) {
        isDeletable = false;
      }
      this.createEditDatabaseModal(dbName, isDeletable);
    },

    search: function () {
      var searchInput,
        searchString,
        strLength,
        reducedCollection;

      searchInput = $('#databaseSearchInput');
      searchString = $('#databaseSearchInput').val();
      reducedCollection = this.collection.filter(
        function (u) {
          return u.get('name').indexOf(searchString) !== -1;
        }
      );
      $(this.el).html(this.template.render({
        collection: reducedCollection,
        searchString: searchString,
        currentDB: this.currentDB,
        readOnly: this.readOnly
      }));
      this.replaceSVGs();

      // after rendering, get the "new" element
      searchInput = $('#databaseSearchInput');
      // set focus on end of text in input field
      strLength = searchInput.val().length;
      searchInput.focus();
      searchInput[0].setSelectionRange(strLength, strLength);
    },

    replaceSVGs: function () {
      $('.svgToReplace').each(function () {
        var img = $(this);
        var id = img.attr('id');
        var src = img.attr('src');
        $.get(src, function (d) {
          var svg = $(d).find('svg');
          svg.attr('id', id)
            .attr('class', 'tile-icon-svg')
            .removeAttr('xmlns:a');
          img.replaceWith(svg);
        }, 'xml');
      });
    },

    remove: function () {
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    evaluateDatabaseName: function (str, substr) {
      var index = str.lastIndexOf(substr);
      return str.substring(0, index);
    },

    createEditDatabaseModal: function (dbName, isDeletable) {
      var buttons = [];
      var tableContent = [];

      tableContent.push(
        window.modalView.createReadOnlyEntry('id_name', 'Name', dbName, '')
      );
      if (isDeletable) {
        buttons.push(
          window.modalView.createDeleteButton(
            'Delete',
            this.submitDeleteDatabase.bind(this, dbName)
          )
        );
      } else {
        buttons.push(window.modalView.createDisabledButton('Delete'));
      }
      window.modalView.show(
        'modalTable.ejs',
        'Delete database',
        buttons,
        tableContent
      );
    },

    createAddDatabaseModal: function () {
      var buttons = [];
      var tableContent = [];

      tableContent.push(
        window.modalView.createTextEntry(
          'newDatabaseName',
          'Name',
          '',
          false,
          'Database Name',
          true,
          [
            {
              rule: Joi.string().regex(/^[a-zA-Z]/),
              msg: 'Database name must start with a letter.'
            },
            {
              rule: Joi.string().regex(/^[a-zA-Z0-9\-_]*$/),
              msg: 'Only Symbols "_" and "-" are allowed.'
            },
            {
              rule: Joi.string().required(),
              msg: 'No database name given.'
            }
          ]
        )
      );

      var users = [];
      window.App.userCollection.each(function (user) {
        users.push({
          value: user.get('user'),
          label: user.get('user')
        });
      });

      tableContent.push(
        window.modalView.createSelectEntry(
          'newUser',
          'Username',
          this.users !== null ? this.users.whoAmI() : 'root',
          'Please define the owner of this database. This will be the only user having ' +
          'initial access to this database if authentication is turned on. Please note ' +
          'that if you specify a username different to your account you will not be ' +
          'able to access the database with your account after having creating it. ' +
          'Specifying a username is mandatory even with authentication turned off. ' +
          'If there is a failure you will be informed.',
          users
        )
      );
      buttons.push(
        window.modalView.createSuccessButton(
          'Create',
          this.submitCreateDatabase.bind(this)
        )
      );
      window.modalView.show(
        'modalTable.ejs',
        'Create Database',
        buttons,
        tableContent
      );

      $('#useDefaultPassword').change(function () {
        if ($('#useDefaultPassword').val() === 'true') {
          $('#row_newPassword').hide();
        } else {
          $('#row_newPassword').show();
        }
      });

      $('#row_newPassword').hide();
    }

  });
}());
