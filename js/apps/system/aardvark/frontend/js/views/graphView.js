/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, forin: true */
/*global require, Backbone, $, _, window, EJS, GraphViewerUI */

window.graphView = Backbone.View.extend({
  el: '#content',

  template: new EJS({url: 'js/templates/graphView.ejs'}),

  initialize: function () {
    var self = this;
    this.newLineTmpl = new EJS({url: "js/templates/graphViewGroupByEntry.ejs"});
    this.graphs = [];
    this.i = 1;
  },

  events: {
    "click input[type='radio'][name='loadtype']": "toggleLoadtypeDisplay",
    "click #createViewer": "createViewer",
    "click #add_group_by": "insertNewAttrLine",
    "click .gv_internal_remove_line": "removeAttrLine"
  },

  removeAttrLine: function(e) {
    var g = $(e.currentTarget)
      .parent()
      .parent(),
      set = g.parent();
    set.get(0).removeChild(g.get(0));
  }, 


  insertNewAttrLine: function() {
    this.i++;
    var next = this.newLineTmpl.render({id: this.i});
    $("#group_by_list").append(next);
  },

  toggleLoadtypeDisplay: function() {
    var selected = $("input[type='radio'][name='loadtype']:checked").attr("id");
    if (selected === "collections") {
      $("#collection_config").css("display", "block");
      $("#graph_config").css("display", "none");
    } else {
      $("#collection_config").css("display", "none");
      $("#graph_config").css("display", "block");
    }
  },

  createViewer: function() {
    var ecol,
      ncol,
      aaconfig,
      undirected,
      randomStart,
      groupByList,
      groupByAttribute,
      label,
      color,
      config,
      ui,
      width,
      self = this;

    undirected = !!$("#undirected").attr("checked");
    label = $("#nodeLabel").val();
    color = $("#nodeColor").val();
    randomStart = !!$("#randomStart").attr("checked");
    
    var graphName;
    var selected = $("input[type='radio'][name='loadtype']:checked").attr("id");
    if (selected === "collections") {
      // selected two individual collections
      ecol = $("#edgeCollection").val();
      ncol = $("#nodeCollection").val();
    }
    else {
      // selected a "graph"
      graphName = $("#graph").val();
      var graph = _.find(this.graphs, function(g) { return g._key === graphName; });

      if (graph) {
        ecol = graph.edges;
        ncol = graph.vertices;
      }

    }

    groupByAttribute = [];
    $("#group_by_list input").each(function() {
      var a = $(this).val();
      if (a) {
        groupByAttribute.push(a);
      }
    });

    aaconfig = {
      type: "arango",
      nodeCollection: ncol,
      edgeCollection: ecol,
      undirected: undirected,
      graph: graphName,
      baseUrl: require("internal").arango.databasePrefix("/")
    };
    
    if (groupByAttribute.length > 0) {
      aaconfig.prioList = groupByAttribute;
    }

    if (label !== undefined && label !== "") {
      config = {
        nodeShaper: {
          label: label
        }
      };
    }
    if (color !== undefined && color !== "") {
      config.nodeShaper = config.nodeShaper || {};
      config.nodeShaper.color = {
        type: "attribute",
        key: color
      };
    }
    width = this.width || $("#content").width();

    $("#background").remove();
    if (randomStart) {
      $.ajax({
        cache: false,
        type: 'PUT',
        url: '/_api/simple/any',
        data: JSON.stringify({
          collection: ncol
        }),
        contentType: "application/json",
        success: function(data) {
          self.ui = new GraphViewerUI($("#content")[0], 
                                      aaconfig, 
                                      width, 
                                      680, 
                                      config, 
                                      (data.document && data.document._id));
        }
      });
    } else {
      self.ui = new GraphViewerUI($("#content")[0], aaconfig, width, 680, config);
    }
  },

  handleResize: function(w) {
    this.width = w;
    if (this.ui) {
      this.ui.changeWidth(w);
    }
  },

  render: function() {
    var self = this;
    $.ajax({
      cache: false,
      type: 'GET',
      url: "/_api/graph",
      contentType: "application/json",
      success: function(data) {
        self.graphs = data.graphs;
        $(self.el).html(self.template.render({
          col: self.collection, 
          gs: _.pluck(self.graphs, "_key")
        }));
        delete self.ui;
      }
    });
    return this;
  }

});
