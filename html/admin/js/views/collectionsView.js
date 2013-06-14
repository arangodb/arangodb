/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, window, setTimeout, clearTimeout, $*/

var collectionsView = Backbone.View.extend({
  el: '#content',
  el2: '.thumbnails',

  searchTimeout: null,

  init: function () {
  },

  template: new EJS({url: 'js/templates/collectionsView.ejs'}),

  render: function () {
    $(this.el).html(this.template.text);
    this.setFilterValues();

    var searchOptions = this.collection.searchOptions;

    $('.thumbnails', this.el).append(
      '<li class="span3"><a href="#new" class="add"><img id="newCollection"'+
      'src="img/plus_icon.png" class="pull-left" />Add Collection</a></li>'
    );
    this.collection.getFiltered(searchOptions).forEach(function (arango_collection) {
      $('.thumbnails', this.el).append(new window.CollectionListItemView({
        model: arango_collection
      }).render().el);
    }, this);

    $('#searchInput').val(searchOptions.searchPhrase);
    $('#searchInput').focus();
    var val = $('#searchInput').val();
    $('#searchInput').val('');
    $('#searchInput').val(val);
    
    return this;
  },
  events: {
    "keydown #searchInput" : "restrictToSearchPhraseKey",
    "change #searchInput"   : "restrictToSearchPhrase",
    "click #searchSubmit"   : "restrictToSearchPhrase",
    "click #checkSystem"    : "checkSystem",
    "click #checkLoaded"    : "checkLoaded",
    "click #checkUnloaded"  : "checkUnloaded",
    "click #checkDocument"  : "checkDocument",
    "click #checkEdge"      : "checkEdge",
    "click #sortName"       : "sortName",
    "click #sortType"       : "sortType",
    "click #sortOrder"      : "sortOrder"
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
  }

});
