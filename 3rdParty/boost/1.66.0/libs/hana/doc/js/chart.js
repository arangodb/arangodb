// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

(function() {
  'use strict';

  var Hana = {};
  Hana.initChart = function(div, options) {
    if (options.xAxis == undefined) {
      options.xAxis = {
        title: { text: "Number of elements" },
        minTickInterval: 1
      };
    }

    if (options.yAxis == undefined) {
      options.yAxis = {
        title: { text: "Time (s)" },
        floor: 0
      };
    }

    if (options.subtitle == undefined) {
      options.subtitle = { text: "(smaller is better)" };
    }

    if (options.chart == undefined) {
      options.chart = { zoomType: 'xy' };
    }

    options.plotOptions = options.plotOptions || {};
    options.plotOptions.series = options.plotOptions.series || {};
    options.plotOptions.series.marker = options.plotOptions.series.marker || { enabled: false };

    if (options.title.x == undefined) {
      options.title.x = -20; // center
    }

    if (options.series.stickyTracking == undefined) {
      options.series.stickyTracking = false;
    }

    // Fix the colors so that a series has the same color on all the charts.
    // Based on the colors at http://api.highcharts.com/highcharts#colors.
    var colorMap = {
        'hana::tuple': '#f45b5b'
      , 'hana::basic_tuple': '#434348'
      , 'mpl::vector': '#90ed7d'
      , 'std::array': '#8085e9'
      , 'fusion::vector': '#f7a35c'
      , 'std::vector: ': '#f15c80'
      , 'std::tuple': '#e4d354'
      , 'hana::set': '#2b908f'
      , 'hana::map': '#7cb5ec'
      , 'fusion::list': '#91e8e1'
    };
    options.series.forEach(function(series) {
      if (colorMap[series.name])
        series.color = colorMap[series.name];
    });

    options.tooltip = options.tooltip || {};
    options.tooltip.valueSuffix = options.tooltip.valueSuffix || 's';

    if (options.legend == undefined) {
      options.legend = {
        layout: 'vertical',
        align: 'right',
        verticalAlign: 'middle',
        borderWidth: 0
      };
    }
    div.highcharts(options);
  };

  window.Hana = Hana;
})();
