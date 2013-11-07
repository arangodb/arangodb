window.DBSelectionView = Backbone.View.extend({
  template: templateEngine.createTemplate("dbSelectionView.ejs"),

  events: {
    "change #dbSelectionList": "changeDatabase"
  },

  initialize: function() {
                var self = this;
    this.collection.fetch({
      success: function(){
        self.render(self.$el);
       } 
    });
    this.current = this.collection.getCurrentDatabase();
  },

  changeDatabase: function(e) {
    var changeTo = $("#dbSelectionList > option:selected").attr("id");
    var url = this.collection.createDatabaseURL(changeTo);
    location.replace(url);
    /*
    var dbname = $(e.currentTarget).text();
    var route =  '/_db/' + encodeURIComponent(dbname) + '/_admin/aardvark/index.html#databases';
    window.location = "http://"+window.location.host + route;
    */
  },

  render: function(el) {
    this.$el = el;
    el.html(this.template.render({
      list: this.collection,
      current: this.current
    }));
    this.delegateEvents();
    return el;
  }
});
