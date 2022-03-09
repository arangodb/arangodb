/* jshint browser: true */
/* jshint unused: false */
/* global arangoHelper, Backbone, templateEngine, $, window, _, nv, d3*/
(function () {
  'use strict';

  window.ShardDistributionView = Backbone.View.extend({
    el: '#content',
    hash: '#distribution',
    template: templateEngine.createTemplate('shardDistributionView.ejs'),
    interval: 10000,

    events: {},

    initialize: function (options) {
      var self = this;
      clearInterval(this.intervalFunction);

      if (window.App.isCluster) {
        // start polling with interval
        this.intervalFunction = window.setInterval(function () {
          if (window.location.hash === self.hash) {
            // self.render(false); //TODO
          }
        }, this.interval);
      }
    },

    remove: function () {
      clearInterval(this.intervalFunction);
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    render: function (navi) {
      if (window.location.hash === this.hash) {
        // pre-render without data (placeholders)
        this.$el.html(this.template.render({}));

        this.fetchOverallStatistics();

        if (navi !== false) {
          arangoHelper.buildClusterSubNav('Distribution');
        }
      }
    },

    rerenderOverallValues: function (data) {
      arangoHelper.renderStatisticsBoxValue('#clusterDatabases', data.databases);
      arangoHelper.renderStatisticsBoxValue('#clusterCollections', data.collections);
      arangoHelper.renderStatisticsBoxValue('#clusterDBServers', data.servers);
      arangoHelper.renderStatisticsBoxValue('#clusterLeaders', data.leaders);
      arangoHelper.renderStatisticsBoxValue('#clusterRealLeaders', data.realLeaders);
      arangoHelper.renderStatisticsBoxValue('#clusterFollowers', data.followers);
      arangoHelper.renderStatisticsBoxValue('#clusterShards', data.shards);
    },

    fetchOverallStatistics: function () {
      var self = this;

      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_admin/cluster/shardStatistics'),
        contentType: 'application/json',
        processData: false,
        async: true,
        success: function (data) {
          self.rerenderOverallValues(data.result);
          // after that, also fetch details now
          self.fetchDetailsStatistics(data.result);
        },
        error: function () {
          arangoHelper.arangoError('Distribution', 'Could not fetch "shardStatistics"');
        }
      });
    },

    rerenderDistributionDonuts: function (donutChartData) {
      let renderDonut = (chartData, idSelector) => {
        nv.addGraph(function () {
          var chart = nv.models.pieChart()
            .x(function (d) {
              return d.label;
            })
            .y(function (d) {
              return d.value;
            })
            .showLabels(true)
            .showTooltipPercent(true)
            .legendPosition("bottom")
            .labelThreshold(0.05)
            .labelType("percent")
            .donut(true)
            .donutRatio(0.35);
          ;

          let id = `${idSelector} svg`;
          d3.select(id)
            .datum(chartData)
            .transition().duration(350)
            .call(chart);

          return chart;
        });
      };

      renderDonut(donutChartData.shards, '#totalDonut');
      renderDonut(donutChartData.leaders, '#leaderDonut');
      renderDonut(donutChartData.followers, '#followerDonut');
    },

    rerenderDistributionTable: function (data) {

      const orderedData = Object.keys(data).sort().reduce(
        (obj, key) => {
          obj[key] = data[key];
          return obj;
        }, {}
      );

      let formatPercent = (number) => {
        return (number * 100).toFixed(2) + " % ";
      };

      $('#shardDistributionTable tbody').html('');
      _.each(orderedData, (info, shortName) => {
        $('#shardDistributionTable tbody').append(
          `
            <tr>
              <td>${shortName}</td>
              <td class="alignRight">${info.shards.total}</td>
              <td class="alignRight">${formatPercent(info.shards.percent)}</td>
              <td class="alignRight">${info.leaders.total}</td>
              <td class="alignRight">${formatPercent(info.leaders.percent)}</td>
              <td class="alignRight">${info.followers.total}</td>
              <td class="alignRight">${formatPercent(info.followers.percent)}</td>
             </tr>
          `
        );
      });
    },

    formatDBServerDetailsData: function (previousData, data) {
      // previousData is our date from our first overall request (needed for upcoming calculations)
      // data is our variable for holding the dbserver specific details.

      let totalShards = previousData.shards;
      let totalLeaders = previousData.leaders;
      let totalFollowers = previousData.followers;

      let donutChartData = {
        shards: [],
        leaders: [],
        followers: []
      }; // format [{label: <string>, value: <number>}]

      let tableData = {}; // format {dbserver: {total: }}

      _.each(data, (info, dbServerId) => {
        let shortName = arangoHelper.getDatabaseShortName(dbServerId);
        tableData[shortName] = {
          shards: {total: 0, percent: 0},
          leaders: {total: 0, percent: 0},
          followers: {total: 0, percent: 0}
        };

        donutChartData.shards.push({
          label: shortName,
          value: (info.shards / totalShards)
        });
        tableData[shortName].shards.total = info.shards;
        tableData[shortName].shards.percent = (info.shards / totalShards);

        donutChartData.leaders.push({
          label: shortName,
          value: (info.leaders / totalLeaders)
        });
        tableData[shortName].leaders.total = info.leaders;
        tableData[shortName].leaders.percent = (info.leaders / totalLeaders);

        donutChartData.followers.push({
          label: shortName,
          value: (info.followers / totalFollowers)
        });
        tableData[shortName].followers.total = info.followers;
        tableData[shortName].followers.percent = (info.followers / totalFollowers);
      });

      this.rerenderDistributionDonuts(donutChartData);
      this.rerenderDistributionTable(tableData);
    },

    fetchDetailsStatistics: function (previousData) {
      var self = this;
      // fetch general overall shardStatistics
      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_admin/cluster/shardStatistics?details=true&DBserver=all'),
        contentType: 'application/json',
        processData: false,
        async: true,
        success: function (data) {
          self.formatDBServerDetailsData(previousData, data.result);
        },
        error: function () {
          arangoHelper.arangoError('Distribution', 'Could not fetch "shardStatistics" details.');
        }
      });
    },

  });
}());
