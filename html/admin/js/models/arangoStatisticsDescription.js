window.StatisticsDescription = Backbone.Model.extend({
  defaults: {
    "figures" : "",
    "groups" : ""
  },
  
  url: function() {
    return "../statistics-description";
  },
  
});
