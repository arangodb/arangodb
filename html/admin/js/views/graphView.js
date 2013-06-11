/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, forin: true */
/*global Backbone, $, window, EJS, GraphViewerUI */

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
      aaconfig,
      undirected,
      label,
      config;
    
    
    ecol = $("#edgeCollection").val();
    ncol = $("#nodeCollection").val();
    undirected = !!$("#undirected").attr("checked");
    label = $("#nodeLabel").val();
    
    aaconfig = {
      type: "arango",
      nodeCollection: ncol,
      edgeCollection: ecol,
      undirected: undirected
    };
    
    if (label !== undefined && label !== "") {
      config = {
        nodeShaper: {
          label: label
        }
      };
    }

    $("#background").remove();
    var ui = new GraphViewerUI($("#content")[0], aaconfig, 940, 680, config);
  },


  render: function() {
    $(this.el).html(this.template.text);
    
    return this;
  }

});
