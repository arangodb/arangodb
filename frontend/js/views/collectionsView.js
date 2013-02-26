var collectionsView = Backbone.View.extend({
  el: '#content',
  el2: '.thumbnails',

  searchOptions: {
    searchPhrase: null,
    includeSystem: false,
    includeLoaded: true,
    includeUnloaded: true
  },

  init: function () {
  },

  template: new EJS({url: '/_admin/html/js/templates/collectionsView.ejs'}),

  render: function () {
    $(this.el).html(this.template.text);
    this.setFilterValues();

    var searchPhrase = '';
    if (this.searchOptions.searchPhrase !== null) {
      searchPhrase = this.searchOptions.searchPhrase.toLowerCase();
    }
    this.collection.each(function (arango_collection) {
      if (arango_collection.get('name').substr(0,1) === "_") {
      }
      if (searchPhrase !== '' && arango_collection.get('name').toLowerCase().indexOf(searchPhrase) === -1) {
        // search phrase entered but current collection does not match?
        return;
      }

      if (this.searchOptions.includeSystem === false && arango_collection.get('isSystem')) {
        // system collection?
        return;
      }
      if (this.searchOptions.includeLoaded === false && arango_collection.get('status') === 'loaded') {
        return;
      }

      if (this.searchOptions.includeUnloaded === false && arango_collection.get('status') === 'unloaded') {
        return;
      }

      $('.thumbnails', this.el).append(new window.CollectionListItemView({model: arango_collection}).render().el);

    }, this);

    $('.thumbnails', this.el).append('<li class="span3"><a href="#new" class="add"><img id="newCollection" src="/_admin/html/img/plus_icon.png" class="pull-left"></img>Add Collection</a></li>');

    $('#searchInput').val(this.searchOptions.searchPhrase);
    $('#searchInput').focus();
    var val = $('#searchInput').val();
    $('#searchInput').val('');
    $('#searchInput').val(val);
    

    return this;
  },
  events: {
    "click .icon-info-sign" : "details",
    "blur #searchInput" : "restrictToSearchPhrase",
    "keypress #searchInput" : "restrictToSearchPhraseKey",
    "change #searchInput" : "restrictToSearchPhrase",
    "click #searchSubmit" : "restrictToSearchPhrase",
    "click #checkSystem" : "checkSystem",
    "click #checkLoaded" : "checkLoaded",
    "click #checkUnloaded" : "checkUnloaded"
  },
  checkSystem: function () {
    var oldValue = this.searchOptions.includeSystem;
    if ($('#checkSystem').is(":checked") === true) {
      this.searchOptions.includeSystem = true;
    }
    else {
      this.searchOptions.includeSystem = false;
    }
    if (oldValue != this.searchOptions.includeSystem) {
      this.render();
    }
  },
  checkLoaded: function () {
    var oldValue = this.searchOptions.includeLoaded;
    if ($('#checkLoaded').is(":checked") === true) {
      this.searchOptions.includeLoaded = true;
    }
    else {
      this.searchOptions.includeLoaded = false;
    }
    if (oldValue != this.searchOptions.includeLoaded) {
      this.render();
    }
  },
  checkUnloaded: function () {
    var oldValue = this.searchOptions.includeUnloaded;
    if ($('#checkUnloaded').is(":checked") === true) {
      this.searchOptions.includeUnloaded = true;
    }
    else {
      this.searchOptions.includeUnloaded = false;
    }
    if (oldValue != this.searchOptions.includeUnloaded) {
      this.render();
    }
  },
  setFilterValues: function () {
    $('#checkLoaded').attr('checked', this.searchOptions.includeLoaded);
    $('#checkUnloaded').attr('checked', this.searchOptions.includeUnloaded);
    $('#checkSystem').attr('checked', this.searchOptions.includeSystem);
  },
  search: function () {
    var searchPhrase = $('#searchInput').val().replace(/(^\s+|\s+$)/g, '');

    if (searchPhrase === this.searchOptions.searchPhrase) {
      return;
    }
    this.searchOptions.searchPhrase = searchPhrase;

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
