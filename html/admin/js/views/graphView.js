window.graphView = Backbone.View.extend({
  el: '#content',
  
  template: new EJS({url: 'js/templates/graphView.ejs'}),
  
  initialize: function () {
    var self = this;

  },

  events: {
    "click #createViewer" : "createViewer"
  },

  createViewer: function() {
    var ecol,
    ncol,
    aaconfig;
    
    ecol = $("#edgeCollection")[0].value;
    ncol = $("#nodeCollection")[0].value;
           
    aaconfig = {
      type: "arango",
      nodeCollection: ncol,
      edgeCollection: ecol
    };

    $("#creationDialog").remove();
    ui = new GraphViewerUI(document.getElementById("content"), aaconfig, 940, 680);
  },


  render: function() {
    $(this.el).html(this.template.text);
    
    return this;
  }

});
