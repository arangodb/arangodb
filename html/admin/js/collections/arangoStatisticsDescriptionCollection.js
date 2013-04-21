window.StatisticsDescription = Backbone.Collection.extend({
  model: window.StatisticsDescription,
  url: "../statistics-description",
  parse: function(response) {
    return response;
  }
});
