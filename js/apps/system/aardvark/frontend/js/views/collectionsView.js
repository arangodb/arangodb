/*jshint browser: true */
/*jshint unused: false */
/*global _, Backbone, templateEngine, window, setTimeout, clearTimeout, arangoHelper, Joi, $*/

(function() {
  "use strict";
  window.CollectionsView = Backbone.View.extend({
    el: '#content',
    el2: '#collectionsThumbnailsIn',

    searchTimeout: null,

    initialize: function () {
    },

    template: templateEngine.createTemplate("collectionsView.ejs"),

    render: function () {
      var dropdownVisible = false;
      if ($('#collectionsDropdown').is(':visible')) {
        dropdownVisible = true;
      }

      $(this.el).html(this.template.render({}));
      this.setFilterValues();

      if (dropdownVisible === true) {
        $('#collectionsDropdown2').show();
      }

      var searchOptions = this.collection.searchOptions;

      this.collection.getFiltered(searchOptions).forEach(function (arango_collection) {
        $('#collectionsThumbnailsIn', this.el).append(new window.CollectionListItemView({
          model: arango_collection,
          collectionsView: this
        }).render().el);
      }, this);

      //if type in collectionsDropdown2 is changed,
      //the page will be rerendered, so check the toggel button
      if($('#collectionsDropdown2').css('display') === 'none') {
        $('#collectionsToggle').removeClass('activated');

      } else {
        $('#collectionsToggle').addClass('activated');
      }

      var length;

      try {
        length = searchOptions.searchPhrase.length;
      }
      catch (ignore) {
      }
      $('#searchInput').val(searchOptions.searchPhrase);
      $('#searchInput').focus();
      $('#searchInput')[0].setSelectionRange(length, length);

      arangoHelper.fixTooltips(".icon_arangodb, .arangoicon", "left");

      return this;
    },

    events: {
      "click #createCollection"   : "createCollection",
      "keydown #searchInput"      : "restrictToSearchPhraseKey",
      "change #searchInput"       : "restrictToSearchPhrase",
      "click #searchSubmit"       : "restrictToSearchPhrase",
      "click #checkSystem"        : "checkSystem",
      "click #checkLoaded"        : "checkLoaded",
      "click #checkUnloaded"      : "checkUnloaded",
      "click #checkDocument"      : "checkDocument",
      "click #checkEdge"          : "checkEdge",
      "click #sortName"           : "sortName",
      "click #sortType"           : "sortType",
      "click #sortOrder"          : "sortOrder",
      "click #collectionsToggle"  : "toggleView",
      "click .css-label"          : "checkBoxes"
    },

    updateCollectionsView: function() {
      var self = this;
      this.collection.fetch({
        success: function() {
          self.render();
        }
      });
    },


    toggleView: function() {
      $('#collectionsToggle').toggleClass('activated');
      $('#collectionsDropdown2').slideToggle(200);
    },

    checkBoxes: function (e) {
      //chrome bugfix
      var clicked = e.currentTarget.id;
      $('#'+clicked).click();
    },

    checkSystem: function () {
      var searchOptions = this.collection.searchOptions;
      var oldValue = searchOptions.includeSystem;

      searchOptions.includeSystem = ($('#checkSystem').is(":checked") === true);

      if (oldValue !== searchOptions.includeSystem) {
        this.render();
      }
    },
    checkEdge: function () {
      var searchOptions = this.collection.searchOptions;
      var oldValue = searchOptions.includeEdge;

      searchOptions.includeEdge = ($('#checkEdge').is(":checked") === true);

      if (oldValue !== searchOptions.includeEdge) {
        this.render();
      }
    },
    checkDocument: function () {
      var searchOptions = this.collection.searchOptions;
      var oldValue = searchOptions.includeDocument;

      searchOptions.includeDocument = ($('#checkDocument').is(":checked") === true);

      if (oldValue !== searchOptions.includeDocument) {
        this.render();
      }
    },
    checkLoaded: function () {
      var searchOptions = this.collection.searchOptions;
      var oldValue = searchOptions.includeLoaded;

      searchOptions.includeLoaded = ($('#checkLoaded').is(":checked") === true);

      if (oldValue !== searchOptions.includeLoaded) {
        this.render();
      }
    },
    checkUnloaded: function () {
      var searchOptions = this.collection.searchOptions;
      var oldValue = searchOptions.includeUnloaded;

      searchOptions.includeUnloaded = ($('#checkUnloaded').is(":checked") === true);

      if (oldValue !== searchOptions.includeUnloaded) {
        this.render();
      }
    },
    sortName: function () {
      var searchOptions = this.collection.searchOptions;
      var oldValue = searchOptions.sortBy;

      searchOptions.sortBy = (($('#sortName').is(":checked") === true) ? 'name' : 'type');
      if (oldValue !== searchOptions.sortBy) {
        this.render();
      }
    },
    sortType: function () {
      var searchOptions = this.collection.searchOptions;
      var oldValue = searchOptions.sortBy;

      searchOptions.sortBy = (($('#sortType').is(":checked") === true) ? 'type' : 'name');
      if (oldValue !== searchOptions.sortBy) {
        this.render();
      }
    },
    sortOrder: function () {
      var searchOptions = this.collection.searchOptions;
      var oldValue = searchOptions.sortOrder;

      searchOptions.sortOrder = (($('#sortOrder').is(":checked") === true) ? -1 : 1);
      if (oldValue !== searchOptions.sortOrder) {
        this.render();
      }
    },

    setFilterValues: function () {
      var searchOptions = this.collection.searchOptions;
      $('#checkLoaded').attr('checked', searchOptions.includeLoaded);
      $('#checkUnloaded').attr('checked', searchOptions.includeUnloaded);
      $('#checkSystem').attr('checked', searchOptions.includeSystem);
      $('#checkEdge').attr('checked', searchOptions.includeEdge);
      $('#checkDocument').attr('checked', searchOptions.includeDocument);
      $('#sortName').attr('checked', searchOptions.sortBy !== 'type');
      $('#sortType').attr('checked', searchOptions.sortBy === 'type');
      $('#sortOrder').attr('checked', searchOptions.sortOrder !== 1);
    },

    search: function () {
      var searchOptions = this.collection.searchOptions;
      var searchPhrase = $('#searchInput').val();
      if (searchPhrase === searchOptions.searchPhrase) {
        return;
      }
      searchOptions.searchPhrase = searchPhrase;

      this.render();
    },

    resetSearch: function () {
      if (this.searchTimeout) {
        clearTimeout(this.searchTimeout);
        this.searchTimeout = null;
      }

      var searchOptions = this.collection.searchOptions;
      searchOptions.searchPhrase = null;
    },

    restrictToSearchPhraseKey: function (e) {
      // key pressed in search box
      var self = this;

      // force a new a search
      this.resetSearch();

      self.searchTimeout = setTimeout(function (){
        self.search();
      }, 200);
    },

    restrictToSearchPhrase: function () {
      // force a new a search
      this.resetSearch();

      // search executed
      this.search();
    },

    createCollection: function(e) {
      e.preventDefault();
      this.createNewCollectionModal();
    },

    submitCreateCollection: function() {
      var collName = $('#new-collection-name').val();
      var collSize = $('#new-collection-size').val();
      var collType = $('#new-collection-type').val();
      var collSync = $('#new-collection-sync').val();
      var shards = 1;
      var shardBy = [];
      if (window.isCoordinator()) {
        shards = $('#new-collection-shards').val();
        if (shards === "") {
          shards = 1;
        }
        shards = parseInt(shards, 10);
        if (shards < 1) {
          arangoHelper.arangoError(
            "Number of shards has to be an integer value greater or equal 1"
          );
          return 0;
        }
        shardBy = _.pluck($('#new-collection-shardBy').select2("data"), "text");
        if (shardBy.length === 0) {
          shardBy.push("_key");
        }
      }
      //no new system collections via webinterface
      //var isSystem = (collName.substr(0, 1) === '_');
      if (collName.substr(0, 1) === '_') {
        arangoHelper.arangoError('No "_" allowed as first character!');
        return 0;
      }
      var isSystem = false;
      var wfs = (collSync === "true");
      if (collSize > 0) {
        try {
          collSize = JSON.parse(collSize) * 1024 * 1024;
        }
        catch (e) {
          arangoHelper.arangoError('Please enter a valid number');
          return 0;
        }
      }
      if (collName === '') {
        arangoHelper.arangoError('No collection name entered!');
        return 0;
      }

      var returnobj = this.collection.newCollection(
        collName, wfs, isSystem, collSize, collType, shards, shardBy
      );
      if (returnobj.status !== true) {
        arangoHelper.arangoError(returnobj.errorMessage);
      }
      this.updateCollectionsView();
      window.modalView.hide();
    },

    createNewCollectionModal: function() {
      var buttons = [],
        tableContent = [],
        advanced = {},
        advancedTableContent = [];

      tableContent.push(
        window.modalView.createTextEntry(
          "new-collection-name",
          "Name",
          "",
          false,
          "",
          true,
          [
            {
              rule: Joi.string().regex(/^[a-zA-Z]/),
              msg: "Collection name must always start with a letter."
            },
            {
              rule: Joi.string().regex(/^[a-zA-Z0-9\-_]*$/),
              msg: 'Only Symbols "_" and "-" are allowed.'
            },
            {
              rule: Joi.string().required(),
              msg: "No collection name given."
            }
          ]
        )
      );
      tableContent.push(
        window.modalView.createSelectEntry(
          "new-collection-type",
          "Type",
          "",
          "The type of the collection to create.",
          [{value: 2, label: "Document"}, {value: 3, label: "Edge"}]
        )
      );
      if (window.isCoordinator()) {
        tableContent.push(
          window.modalView.createTextEntry(
            "new-collection-shards",
            "Shards",
            "",
            "The number of shards to create. You cannot change this afterwards. "
              + "Recommended: DBServers squared",
            "",
            true
          )
        );
        tableContent.push(
          window.modalView.createSelect2Entry(
            "new-collection-shardBy",
            "shardBy",
            "",
            "The keys used to distribute documents on shards. "
              + "Type the key and press return to add it.",
            "_key",
            false
          )
        );
      }
      buttons.push(
        window.modalView.createSuccessButton(
          "Save",
          this.submitCreateCollection.bind(this)
        )
      );
      advancedTableContent.push(
        window.modalView.createTextEntry(
          "new-collection-size",
          "Journal size",
          "",
          "The maximal size of a journal or datafile (in MB). Must be at least 1.",
          "",
          false,
          [
            {
              rule: Joi.string().required(),
              msg: "No journal size given."
            },
            {
              rule: Joi.string().regex(/^[0-9]*$/),
              msg: "Must be a number."
            }
          ]
      )
      );
      advancedTableContent.push(
        window.modalView.createSelectEntry(
          "new-collection-sync",
          "Sync",
          "",
          "Synchronise to disk before returning from a create or update of a document.",
          [{value: false, label: "No"}, {value: true, label: "Yes"}]
        )
      );
      advanced.header = "Advanced";
      advanced.content = advancedTableContent;
      window.modalView.show(
        "modalTable.ejs",
        "New Collection",
        buttons,
        tableContent,
        advanced
      );
    }

  });
}());
