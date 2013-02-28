var collectionsView = Backbone.View.extend({
  el: '#content',
  el2: '.thumbnails',

  init: function () {
  },

  template: new EJS({url: '/_admin/html/js/templates/collectionsView.ejs'}),

  render: function () {
    $(this.el).html(this.template.text);
    this.setFilterValues();

    var searchOptions = this.collection.searchOptions;

    this.collection.getFiltered(searchOptions).forEach(function (arango_collection) {
      $('.thumbnails', this.el).append(new window.CollectionListItemView({model: arango_collection}).render().el);
    }, this);

    $('.thumbnails', this.el).append('<li class="span3"><a href="#new" class="add"><img id="newCollection" src="/_admin/html/img/plus_icon.png" class="pull-left"></img>Add Collection</a></li>');
    $('#searchInput').val(searchOptions.searchPhrase);
    $('#searchInput').focus();
    var val = $('#searchInput').val();
    $('#searchInput').val('');
    $('#searchInput').val(val);
    

    return this;
  },
  events: {
    "click .icon-info-sign" : "details",
    //"blur #searchInput" : "restrictToSearchPhrase",
    "keypress #searchInput" : "restrictToSearchPhraseKey",
    "change #searchInput"   : "restrictToSearchPhrase",
    "click #searchSubmit"   : "restrictToSearchPhrase",
    "click #checkSystem"    : "checkSystem",
    "click #checkLoaded"    : "checkLoaded",
    "click #checkUnloaded"  : "checkUnloaded",
    "click #checkDocument"  : "checkDocument",
    "click #checkEdge"      : "checkEdge"
  },
  checkSystem: function () {
    var searchOptions = this.collection.searchOptions;
    var oldValue = searchOptions.includeSystem;

    searchOptions.includeSystem = ($('#checkSystem').is(":checked") === true);

    if (oldValue != searchOptions.includeSystem) {
      this.render();
    }
  },
  checkEdge: function () {
    var searchOptions = this.collection.searchOptions;
    var oldValue = searchOptions.includeEdge;

    searchOptions.includeEdge = ($('#checkEdge').is(":checked") === true);

    if (oldValue != searchOptions.includeEdge) {
      this.render();
    }
  },
  checkDocument: function () {
    var searchOptions = this.collection.searchOptions;
    var oldValue = searchOptions.includeDocument;

    searchOptions.includeDocument = ($('#checkDocument').is(":checked") === true);

    if (oldValue != searchOptions.includeDocument) {
      this.render();
    }
  },
  checkLoaded: function () {
    var searchOptions = this.collection.searchOptions;
    var oldValue = searchOptions.includeLoaded;

    searchOptions.includeLoaded = ($('#checkLoaded').is(":checked") === true);

    if (oldValue != searchOptions.includeLoaded) {
      this.render();
    }
  },
  checkUnloaded: function () {
    var searchOptions = this.collection.searchOptions;
    var oldValue = searchOptions.includeUnloaded;

    searchOptions.includeUnloaded = ($('#checkUnloaded').is(":checked") === true);

    if (oldValue != searchOptions.includeUnloaded) {
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
  },
  search: function () {
    var searchOptions = this.collection.searchOptions;
    var searchPhrase = $('#searchInput').val().replace(/(^\s+|\s+$)/g, '');

    if (searchPhrase === searchOptions.searchPhrase) {
      return;
    }
    searchOptions.searchPhrase = searchPhrase;

    this.render();
  },

  restrictToSearchPhraseKey: function (e) {
    // key pressed in search box
    var self = this;
    if (e.keyCode == 13) {
      e.preventDefault();
      // return pressed? this triggers the search
      this.search();
    }
    setTimeout(function (){
      self.search();
    }, 200);
  },

  restrictToSearchPhrase: function () {
    // search executed 
    this.search();
  },

  details: function () {
  }

});
