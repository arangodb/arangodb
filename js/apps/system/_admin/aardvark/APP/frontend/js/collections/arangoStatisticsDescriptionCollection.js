window.StatisticsDescriptionCollection = Backbone.Collection.extend({
  model: window.StatisticsDescription,
  url: '/_admin/statistics-description',
  parse: function (response) {
    return response;
  }
});
