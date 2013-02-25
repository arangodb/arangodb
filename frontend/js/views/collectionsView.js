var collectionsView = Backbone.View.extend({
  el: '#content',
  el2: '.thumbnails',

  searchOptions: {
    searchPhrase: null,
    excludeSystem: false
  },

  init: function () {
  },

  template: new EJS({url: '/_admin/html/js/templates/collectionsView.ejs'}),

  render: function () {
    $(this.el).html(this.template.text);

	$('.thumbnails', this.el).append('<li class="span3"><a href="#new" class="add"><img id="newCollection" src="/_admin/html/img/plus_icon.png" class="pull-left"></img> Neu hinzufuegen</a></li>'
	);

    var searchPhrase = '';
    if (this.searchOptions.searchPhrase !== null) {
      searchPhrase = this.searchOptions.searchPhrase.toLowerCase();
    }
    this.collection.each(function (arango_collection) {
      if (searchPhrase !== '' && arango_collection.get('name').toLowerCase().indexOf(searchPhrase) === -1) {
        // search phrase entered but current collection does not match?
        return;
      }

      if (this.searchOptions.excludeSystem && arango_collection.get('isSystem')) {
        // system collection?
        return;
      }

      $('.thumbnails', this.el).append(new window.CollectionListItemView({model: arango_collection}).render().el);

    }, this);

    $('#searchInput').val(this.searchOptions.searchPhrase);
    $('#searchInput').focus();

    return this;
  },
  events: {
    "click .icon-info-sign" : "details",
    "blur #searchInput" : "restrictToSearchPhrase",
    "keypress #searchInput" : "restrictToSearchPhraseKey",
    "click #searchSubmit" : "restrictToSearchPhrase"
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
    if (e.keyCode == 13) {
      e.preventDefault();
      // return pressed? this triggers the search
      this.search();
    }
  },

  restrictToSearchPhrase: function () {
    // search executed 
    this.search();
  },

  details: function () {
  }

});
