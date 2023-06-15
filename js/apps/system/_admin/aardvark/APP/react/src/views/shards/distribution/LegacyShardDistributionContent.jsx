/* global $, _, nv, d3 arangoHelper */
import React from 'react';

function fetchOverallStatistics() {
  $.ajax({
    type: 'GET',
    cache: false,
    url: arangoHelper.databaseUrl('/_admin/cluster/shardStatistics'),
    contentType: 'application/json',
    processData: false,
    async: true,
    success: function (data) {
      rerenderOverallValues(data.result);
      // after that, also fetch details now
      fetchDetailsStatistics(data.result);
    },
    error: function () {
      arangoHelper.arangoError('Distribution', 'Could not fetch "shardStatistics"');
    }
  });
}

function rerenderOverallValues(data) {
  arangoHelper.renderStatisticsBoxValue('#clusterDatabases', data.databases);
  arangoHelper.renderStatisticsBoxValue('#clusterCollections', data.collections);
  arangoHelper.renderStatisticsBoxValue('#clusterDBServers', data.servers);
  arangoHelper.renderStatisticsBoxValue('#clusterLeaders', data.leaders);
  arangoHelper.renderStatisticsBoxValue('#clusterRealLeaders', data.realLeaders);
  arangoHelper.renderStatisticsBoxValue('#clusterFollowers', data.followers);
  arangoHelper.renderStatisticsBoxValue('#clusterShards', data.shards);
}

function fetchDetailsStatistics(previousData) {
  // fetch general overall shardStatistics
  $.ajax({
    type: 'GET',
    cache: false,
    url: arangoHelper.databaseUrl('/_admin/cluster/shardStatistics?details=true&DBserver=all'),
    contentType: 'application/json',
    processData: false,
    async: true,
    success: function (data) {
      formatDBServerDetailsData(previousData, data.result);
    },
    error: function () {
      arangoHelper.arangoError('Distribution', 'Could not fetch "shardStatistics" details.');
    }
  });
}

function formatDBServerDetailsData(previousData, data) {
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
      shards: { total: 0, percent: 0 },
      leaders: { total: 0, percent: 0 },
      followers: { total: 0, percent: 0 }
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

  rerenderDistributionDonuts(donutChartData);
  rerenderDistributionTable(tableData);
}

function rerenderDistributionDonuts(donutChartData) {
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
}

function rerenderDistributionTable(data) {

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
}

export const LegacyShardDistributionContent = ({refetchToken}) => {
  React.useEffect(() => {
    fetchOverallStatistics();
  }, [refetchToken]);

  return (
    <div id="shardDistributionContent" className="innerContent shardDistributionContent">

      <div className="pure-g cluster-values">

        <div className="pure-u-1-2 pure-u-md-1-4">
          <div className="valueWrapper">
            <div id="clusterDatabases" className="value"><i className="fa fa-spin fa-circle-o-notch"
              style={{ color: "rgba(0, 0, 0, 0.64)" }}></i></div>
            <div className="graphLabel">databases</div>
          </div>
        </div>

        <div className="pure-u-1-2 pure-u-md-1-4">
          <div className="valueWrapper">
            <div id="clusterCollections" className="value"><i className="fa fa-spin fa-circle-o-notch"
              style={{ color: "rgba(0, 0, 0, 0.64)" }}></i></div>
            <div className="graphLabel">collections</div>
          </div>
        </div>

        <div className="pure-u-1-2 pure-u-md-1-4">
          <div className="valueWrapper">
            <div id="clusterShards" className="value"><i className="fa fa-spin fa-circle-o-notch"
              style={{ color: "rgba(0, 0, 0, 0.64)" }}></i></div>
            <div className="graphLabel">total shards</div>
          </div>
        </div>

        <div className="pure-u-1-2 pure-u-md-1-4">
          <div className="valueWrapper">
            <div id="clusterDBServers" className="value"><i className="fa fa-spin fa-circle-o-notch"
              style={{ color: "rgba(0, 0, 0, 0.64)" }}></i></div>
            <div className="graphLabel">db servers with shards</div>
          </div>
        </div>

        <div className="pure-u-1-2 pure-u-md-1-4">
          <div className="valueWrapper">
            <div id="clusterLeaders" className="value"><i className="fa fa-spin fa-circle-o-notch"
              style={{ color: "rgba(0, 0, 0, 0.64)" }}></i></div>
            <div className="graphLabel">leader shards</div>
          </div>
        </div>

        <div className="pure-u-1-2 pure-u-md-1-4">
          <div className="valueWrapper">
            <div id="clusterRealLeaders" className="value"><i className="fa fa-spin fa-circle-o-notch"
              style={{ color: "rgba(0, 0, 0, 0.64)" }}></i></div>
            <div className="graphLabel">shard group leader shards</div>
          </div>
        </div>

        <div className="pure-u-1-2 pure-u-md-1-4">
          <div className="valueWrapper">
            <div id="clusterFollowers" className="value"><i className="fa fa-spin fa-circle-o-notch"
              style={{ color: "rgba(0, 0, 0, 0.64)" }}></i></div>
            <div className="graphLabel">follower shards</div>
          </div>
        </div>

      </div>

      <div className="pure-g cluster-values">
        <div className="pure-u-1-1">
          <div className="header">Shard Distribution</div>
        </div>

        <div className="pure-u-1-1 pure-u-sm-1-1 pure-u-md-1-3">
          <div className="subHeader graphLabel">Total Shard Distribution</div>
          <div id="totalDonut" className="svgWrapper">
            <svg></svg>
          </div>
        </div>

        <div className="pure-u-1-1 pure-u-sm-1-1 pure-u-md-1-3">
          <div className="subHeader graphLabel">Leader Shard Distribution</div>
          <div id="leaderDonut" className="svgWrapper">
            <svg></svg>
          </div>
        </div>

        <div className="pure-u-1-1 pure-u-sm-1-1 pure-u-md-1-3">
          <div className="subHeader graphLabel">Follower Shard Distribution</div>
          <div id="followerDonut" className="svgWrapper">
            <svg></svg>
          </div>
        </div>

        <div className="pure-u-1-1">
          <table className="pure-table" id="shardDistributionTable">
            <thead>
              <tr>
                <th>DB Server</th>
                <th>Total (absolute)</th>
                <th>Total (percent)</th>
                <th>Leaders (absolute)</th>
                <th>Leaders (percent)</th>
                <th>Followers (absolute)</th>
                <th>Followers (percent)</th>
              </tr>
            </thead>
            <tbody>
            </tbody>
          </table>
        </div>

      </div>
    </div>
  );
};
