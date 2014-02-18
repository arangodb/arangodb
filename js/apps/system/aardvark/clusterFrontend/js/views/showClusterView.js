/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, plannerTemplateEngine, alert */

(function() {
  "use strict";

  window.ShowClusterView = Backbone.View.extend({

    el: "#content",

    template: clusterTemplateEngine.createTemplate("showCluster.ejs"),

    initialize: function() {
      this.dbservers = new window.ClusterServers();
      this.dbservers.fetch({
        async : false
      });
      this.coordinators = new window.ClusterCoordinators();
      this.coordinators.fetch({
        async : false
      });
      if (this.statisticsDescription === undefined) {
        this.statisticsDescription = new window.StatisticsDescription();
        this.statisticsDescription.fetch({
          async:false
        });
      }
      var self = this;
      var statCollect = new window.ClusterStatisticsCollection();
      this.dbservers.forEach(function (dbserver) {
        var stat = new window.Statistics({name: dbserver.id});
        stat.url = dbserver.get("address").replace("tcp", "http") + "/_admin/statistics";
        statCollect.add(stat);
      });

      this.coordinators.forEach(function (coordinator) {
        var stat = new window.Statistics({name: coordinator.id});
        stat.url = coordinator.get("address").replace("tcp", "http") + "/_admin/statistics";
        statCollect.add(stat);
      });
      statCollect.fetch();
      this.data = statCollect;

      var typeModel = new window.ClusterType();
      typeModel.fetch({
        async: false
      });
      this.type = typeModel.get("type");
    },

    listByAddress: function() {
      var byAddress = this.dbservers.byAddress();
      byAddress = this.coordinators.byAddress(byAddress);
      return byAddress;
    },

    render: function() {
      var self = this;
      console.log(this.listByAddress());
      $(this.el).html(this.template.render({
        byAddress: this.listByAddress(),
        type: this.type
      }));
      var totalTimeData = [];
      this.data.forEach(function(m) {
        totalTimeData.push({key: m.get("name"), value :m.get("client").totalTime.sum});
      });
      self.renderPieChart(totalTimeData);
    },



    renderPieChart: function(dataset) {
      var w = 620;
      var h = 480;



      var radius = Math.min(w, h) / 2; //change 2 to 1.4. It's hilarious.

      var color = d3.scale.category20();

      var arc = d3.svg.arc() //each datapoint will create one later.
      .outerRadius(radius - 20)
      .innerRadius(0);

      var pie = d3.layout.pie()
      .sort(function (d) {
        return d.value;
      })
      .value(function (d) {
        return d.value;
      });

      var svg = d3.select("#clusterGraphs").append("svg")
      .attr("width", w)
      .attr("height", h)
      .attr("class", "clusterChart")
      .append("g") //someone to transform. Groups data.
      .attr("transform", "translate(" + w / 2 + "," + h / 2 + ")");

      var slices = svg.selectAll(".arc")
      .data(pie(dataset))
      .enter().append("g")
      .attr("class", "slice");

      slices.append("path")
      .attr("d", arc)
      .style("fill", function (d, i) {
        return color(i);
      });

      //add text, even
      slices.append("text")
      .attr("transform", function (d) {
        return "translate(" + arc.centroid(d) + ")";
      })
      .attr("class", "data-title")
      .text(function (d) {
        return d.data.key;
      });
    }
  });



}());
