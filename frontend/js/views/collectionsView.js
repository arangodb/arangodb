var collectionsView = Backbone.View.extend({
  el: '#content',
  el2: '.thumbnails',
  searchPhrase: '',

  init: function () {
  },

  template: new EJS({url: '/_admin/html/js/templates/collectionsView.ejs'}),

  render: function() {
    $(this.el).html(this.template.text);

    var searchPhrase = this.searchPhrase.toLowerCase();

    this.collection.each(function (arango_collection) {
      if (searchPhrase !== '' && arango_collection.get('name').toLowerCase().indexOf(searchPhrase) === -1) {
        return;
      }

      $('.thumbnails', this.el).append(new window.CollectionListItemView({model: arango_collection}).render().el);
    }, this);

    $('#searchInput').val(this.searchPhrase);
    $('#searchInput').focus();

    return this;
  },
  events: {
    "click .icon-info-sign" : "details",
    "blur #searchInput" : "restrictToSearchPhrase",
    "keypress #searchInput" : "restrictToSearchPhraseKey",
    "click #searchSubmit" : "restrictToSearchPhrase"
  },

  restrictToSearchPhraseKey: function(e) {
    if (e.keyCode == 13) {
      this.searchPhrase = $('#searchInput').val().replace(/(^\s+|\s+$)/g, '');
      this.render();
    }
  },
  restrictToSearchPhrase: function() {
    var searchPhrase = this.searchPhrase;
    this.searchPhrase = $('#searchInput').val().replace(/(^\s+|\s+$)/g, '');
    if (searchPhrase === this.searchPhrase) {
      return;
    }
    this.render();
  },

  details: function() {
  }

});
