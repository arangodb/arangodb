window.StatisticsCollection = Backbone.Collection.extend({
  model: window.Statistics,
  url: '/_admin/statistics'
});
