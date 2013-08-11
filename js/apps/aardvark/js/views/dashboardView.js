/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, flush, window, arangoHelper, nv, d3, localStorage*/

var dashboardView = Backbone.View.extend({
  el: '#content',
  updateInterval: 500, // 0.5 second, constant
  updateFrequency: 10, // the actual update rate (5 s)
  updateCounter: 0,
  arraySize: 20, // how many values will we keep per figure?
  seriesData: {},
  charts: {},
  units: [],
  graphState: {},
  updateNOW: false,
  collectionsStats: {
    "corrupted": 0,
    "new born collection" : 0,
    "unloaded" : 0,
    "loaded" : 0,
    "in the process of being unloaded" : 0,
    "deleted" : 0
  },
  detailGraph: "userTime",

  initialize: function () {
    this.loadGraphState();
    this.arangoReplication = new window.ArangoReplication();
    var self = this;

    this.initUnits();
    //this.addCustomCharts();

    this.collection.fetch({
      success: function() {
        self.countCollections();
        self.calculateSeries();
        self.renderCharts();

        window.setInterval(function() {
          self.updateCounter++;

          if (self.updateNOW === true) {
            self.calculateSeries();
            self.renderCharts();
            self.updateNOW = false;
          }

          if (self.updateCounter < self.updateFrequency) {
            return false;
          }
          if (window.location.hash === '#dashboard') {
            self.getReplicationStatus();
          }

          self.updateCounter = 0;
          self.collection.fetch({
            success: function() {
              self.calculateSeries();
              self.renderCharts();
            },
            error: function() {
              // need to flush previous values
              self.calculateSeries(true);
              arangoHelper.arangoError("Lost connection to database!");
              self.renderCharts();
            }
          });

        }, self.updateInterval);
      }
    });
  },

  events: {
    "click .dashboard-dropdown li" : "checkEnabled",
    "click .interval-dropdown li"  : "checkInterval",
    "click .db-zoom"               : "renderDetailChart",
    "click .db-minimize"           : "checkDetailChart",
    "click .db-hide"               : "hideChart",
    "click .group-close"           : "hideGroup",
    "click .group-open"            : "showGroup",
    "click .dashSwitch"            : "showCategory"
  },

  template: new EJS({url: 'js/templates/dashboardView.ejs'}),

  countCollections: function() {
    var self = this;
    $.each(window.arangoCollectionsStore.models, function(k,v) {
      if ( self.collectionsStats[this.attributes.status] === undefined ) {
        self.collectionsStats[this.attributes.status] = 0;
      }
      self.collectionsStats[this.attributes.status]++;
    });
  },

  getReplicationStatus: function () {
    this.replLogState = this.arangoReplication.getLogState();
    this.replApplyState = this.arangoReplication.getApplyState();
    this.putReplicationStatus();
  },

  putReplicationStatus: function () {
    var loggerRunning  = this.replLogState.state.running;
    var applierRunning = this.replApplyState.state.running;

    if (applierRunning || this.replApplyState.state.lastError !== '') {
      $('#detailReplication').height(290);
      $('.checkApplyRunningStatus').show();
    }
    else {
      $('.checkApplyRunningStatus').hide();
    }

    var time = this.replLogState.state.time;

    var cls = loggerRunning ? 'true' : 'false';
    var runningLog = '<div class="' + cls + 'Class">' + cls + '</div>';

    var clientString = '-';
    if (this.replLogState.state.clients) {
      $.each(this.replLogState.state.clients, function(k,v) {
        clientString = clientString + "Server: " + v.serverId + " | Time: " + v.time + "\n";
      });
    }

    var lastLog;
    if (this.replLogState.state.lastLogTick === '0') {
      lastLog = '-';
    }

    var numEvents = this.replLogState.state.totalEvents || 0;

    //log table
    $('#logRunningVal').html(runningLog);
    $('#logTotalEventsVal').text(numEvents);
    $('#logTimeVal').text(time);
    $('#logLastTickVal').text(lastLog);
    $('#logClientsVal').text(clientString);


    //apply table
    var lastAppliedTick = "-";
    var lastAvailableTick = "-";
    var progress = "-";
    var lastError = "-";
    var endpoint = "-";
    var numRequests = "-";
    var numFailed = "-";

    if (this.replApplyState.state.lastAppliedContinuousTick !== null) {
      lastAppliedTick = this.replApplyState.state.lastAppliedContinuousTick;
    }

    if (this.replApplyState.state.lastAvailableContinuousTick !== null) {
      lastAvailableTick = this.replApplyState.state.lastAvailableContinuousTick;
    }

    if (this.replApplyState.state.endpoint !== undefined) {
      endpoint = this.replApplyState.state.endpoint;
    }

    time = this.replApplyState.state.time;

    if (this.replApplyState.state.progress) {
      progress = this.replApplyState.state.progress.message;
    }

    if (this.replApplyState.state.lastError) {
      lastError = this.replApplyState.state.lastError.errorMessage;
    }

    cls = applierRunning ? 'true' : 'false';
    var runningApply = '<div class="' + cls + 'Class">' + cls + '</div>';

    numEvents = this.replApplyState.state.totalEvents || 0;
    numRequests = this.replApplyState.state.totalRequests || 0;
    numFailed = this.replApplyState.state.totalFailedConnects || 0;

    $('#applyRunningVal').html(runningApply);
    $('#applyEndpointVal').text(endpoint);
    $('#applyLastAppliedTickVal').text(lastAppliedTick);
    $('#applyLastAvailableTickVal').text(lastAvailableTick);
    $('#applyTimeVal').text(time);
    $('#applyProgressVal').text(progress);
    $('#applyTotalEventsVal').text(numEvents);
    $('#applyTotalRequestsVal').text(numRequests);
    $('#applyTotalFailedVal').text(numFailed);
    $('#applyLastErrorVal').text(lastError);
  },

  render: function() {
    var self = this;
    self.updateNOW = true;
    $(this.el).html(this.template.text);
    this.getReplicationStatus();

    //Client calculated charts
    /*self.genCustomCategories();
    self.genCustomChartDescription(
      "userTime + systemTime",
      "custom",
      "totalTime2",
      "Total Time (User+System)",
      "accumulated",
      "seconds"
    );*/

    var counter = 1;
    $.each(this.options.description.models[0].attributes.groups, function () {
      $('.thumbnails').append(
        '<ul class="statGroups" id="' + this.group + '">' +
        '<i class="group-close icon-minus icon-white"></i>' +
        '<div id="statsHeaderDiv"><h4 class="statsHeader">' + this.name + '</h4></div>' +
        '</ul>');

      //group
      $('#menuGroups').append('<li class="nav-header">' + this.name + '</li>');
      $('#menuGroups').append('<li class="divider" id="' + this.group + 'Divider"></li>');

      /*TEST
      $('#menuGroups').append(
        '<li class="dropdown-submenu pull-left"><a tabindex="-1" href="#">'+this.name+'</a>'+
        '<ul id="' + this.group + 'Divider" class="dropdown-menu graphDropdown"></ul>'
      );
      */


      //group entries
      if (self.options.description.models[0].attributes.groups.length === counter) {
        $('#'+this.group+'Divider').addClass('dbNotVisible');
      }
      counter++;
    });

    $.each(this.options.description.models[0].attributes.figures, function () {
      self.renderFigure(this);
    });
    $('#every'+self.updateFrequency+'seconds').prop('checked',true);

    if (this.collection.models[0] === undefined) {
      this.collection.fetch({
        success: function() {
          self.calculateSeries();
          self.renderCharts();
        },
        error: function() {
          self.calculateSeries();
          self.renderCharts(flush);
        }
      });
    }
    else {
      self.calculateSeries();
      self.renderCharts();
    }

    this.loadGraphState();
    return this;
  },

  addCustomCharts: function () {
    var self = this;
    var figure = {
      "description" : "my custom chart",
      "group" : "custom",
      "identifier" : "custom1",
      "name" : "Custom1",
      "type" : "accumulated",
      "units" : "seconds",
      "exec" : function () {
        var val1 = self.collection.models[0].attributes.system.userTime;
        var val2 = self.collection.models[0].attributes.system.systemTime;
        var totalTime2Value = val1+val2;
        return totalTime2Value;
      }
    };

    var addGroup = true;

    $.each(this.options.description.models[0].attributes.groups, function(k, v) {
      if (self.options.description.models[0].attributes.groups[k].group === figure.group) {
        addGroup = false;
      }
    });

    if (addGroup === true) {
      self.options.description.models[0].attributes.groups.push({
        "description" : "custom",
        "group" : "custom",
        "name" : "custom"
      });
    }

    this.options.description.models[0].attributes.figures.push(figure);
  },

  checkInterval: function (a) {
    var self = this;
    this.updateFrequency = a.target.value;
    self.calculateSeries();
    self.renderCharts();
  },

  checkEnabled: function (a) {
    var myId = a.target.id;
    var position = myId.search('Checkbox');
    var preparedId = myId.substring(0, position);
    var toCheck = $(a.target).is(':checked');
    if (toCheck === false) {
      $("#" + preparedId).hide();
    }
    else if (toCheck === true) {
      $("#" + preparedId).show();
    }
    this.saveGraphState();
  },

  initUnits : function () {
    this.units = [ ];
    var i;
    var scale = 0.0001;

    for (i = 0; i < 12; ++i) {
      this.units.push(scale * 2);
      this.units.push(scale * 2.5);
      this.units.push(scale * 4);
      this.units.push(scale * 5);
      scale *= 10;
    }
  },

  getMaxValue : function (identifier) {
    var max = this.seriesData[identifier].max || 1;

    var i = 0, n = this.units.length;
    while (i < n) {
      var unit = this.units[i++];
      if (max === unit) {
        break;
      }
      if (max < unit) {
        return unit;
      }
    }

    return max;
  },

  checkDetailChart: function (a) {
    if ($(a.target).hasClass('icon-minus') === true) {
      $('#detailGraph').height(43);
      $('#detailGraphChart').hide();
      $(a.target).removeClass('icon-minus');
      $(a.target).addClass('icon-plus');
    }
    else {
      $('#detailGraphChart').show();
      $('#detailGraph').height(300);
      $(a.target).removeClass('icon-plus');
      $(a.target).addClass('icon-minus');
    }
  },

  hideGroup: function (a) {
    var group = $(a.target).parent();
    $(a.target).removeClass('icon-minus group-close');
    $(a.target).addClass('icon-plus group-open');
    $(group).addClass("groupHidden");
  },

  showCategory: function (e) {
    var id = e.target.id;
    if (id === 'replSwitch') {
      $('#statSwitch').removeClass('activeSwitch');
      $('#'+id).addClass('activeSwitch');
      $('#detailGraph').hide();
      $('.statGroups').hide();
      $('#detailReplication').show();
    }
    else if (id === 'statSwitch') {
      $('#'+id).addClass('activeSwitch');
      $('#replSwitch').removeClass('activeSwitch');
      $('#detailReplication').hide();
      $('#detailGraph').show();
      $('.statGroups').show();
    }
  },

  showGroup: function (a) {
    var group = $(a.target).parent();
    $(a.target).removeClass('icon-plus group-open');
    $(a.target).addClass('icon-minus group-close');
    $(group).removeClass("groupHidden");
  },

  hideChart: function (a) {
    var figure = $(a.target).attr("value");
    $('#'+figure+'Checkbox').prop('checked', false);
    $('#'+figure).hide();
    this.saveGraphState();
  },

  renderDetailChart: function (a) {
    var self = this;
    self.detailGraph = $(a.target).attr("value");
    $.each(this.options.description.models[0].attributes.figures, function () {
      if(this.identifier === self.detailGraph) {
        $('#detailGraphHeader').text(this.name);
        $("html, body").animate({ scrollTop: 0 }, "slow");
        $('#detailGraphChart').show();
        $('#detailGraph').height(300);
        $('#dbHideSwitch').addClass('icon-minus');
        $('#dbHideSwitch').removeClass('icon-plus');
        self.updateNOW = true;
        self.calculateSeries();
        self.renderCharts();
      }
    });
  },

  renderCollectionsChart: function () {
    var self = this;
    nv.addGraph(function() {
      var chart = nv.models.pieChart()
      .x(function(d) { return d.label; })
      .y(function(d) { return d.value; })
      .showLabels(true);

      d3.select("#detailCollectionsChart svg")
      .datum(self.convertCollections())
      .transition().duration(1200)
      .call(chart);

      return chart;
    });

  },

  convertCollections: function () {
    var self = this;
    var collValues = [];
    $.each(self.collectionsStats, function(k,v) {
      collValues.push({
        "label" : k,
        "value" : v
      });
    });

    return [{
      key: "Collections Status",
      values: collValues
    }];
  },

  renderCharts: function () {
    var self = this;
    $('#every'+self.updateFrequency+'seconds').prop('checked',true);
    //self.renderCollectionsChart();

    $.each(self.options.description.models[0].attributes.figures, function () {
      var figure = this;
      var identifier = figure.identifier;
      var chart;

      if (self.charts[identifier] === undefined) {
        chart = self.charts[identifier] = nv.models.lineChart();
        chart.xAxis.axisLabel('').tickFormat(function (d) {
          if (isNaN(d)) {
            return '';
          }

          function pad (value) {
            return (value < 10 ? "0" + String(value) : String(value));
          }

          var date = new Date(d / 10);

          return pad(date.getHours()) + ":" + pad(date.getMinutes()) + ":" + pad(date.getSeconds());
        });

        var label = figure.units;

        if (figure.units === 'bytes') {
          label = "megabytes";
        }
        chart.yAxis.axisLabel(label);

        nv.addGraph (function () {
          nv.utils.windowResize(function () {
            d3.select('#' + identifier + 'Chart svg').call(chart);
          });

          return chart;
        });
      }
      else {
        chart = self.charts[identifier];
      }

      chart.yDomain([ 0, self.getMaxValue(identifier) ]);

      if (self.detailGraph === undefined) {
        self.detailGraph = "userTime";
      }
      if (self.detailGraph === identifier) {
        d3.select("#detailGraphChart svg")
        .call(chart)
        .datum([{
          values: self.seriesData[identifier].values,
          key: identifier,
          color: "#8AA051" 
        }])
        .transition().duration(500);
      }

      //disable ticks/label for small charts

      d3.select("#" + identifier + "Chart svg")
      .call(chart)
      .datum([ { values: self.seriesData[identifier].values, key: identifier, color: "#8AA051" } ])
      .transition().duration(500);
    });
    this.loadGraphState();
  },

  calculateSeries: function (flush) {
    var self = this;

    var timeStamp = Math.round(new Date() * 10);

    $.each(self.options.description.models[0].attributes.figures, function () {
      var figure = this;
      var identifier = figure.identifier;

      if (self.seriesData[identifier] === undefined) {
        self.seriesData[identifier] = { max: 0, values: [ ] };
      }

      while (self.seriesData[identifier].values.length > self.arraySize) {
        self.seriesData[identifier].values.shift();
      }

      if (flush) {
        self.seriesData[identifier].values.push({ x: timeStamp, y: undefined, value: undefined });
        return;
      }

      var responseValue;

      if (figure.exec) {
        responseValue = figure.exec();
      }
      else {
        responseValue = self.collection.models[0].attributes[figure.group][identifier];
      }

      if (responseValue !== undefined && responseValue !== null) {
        if (responseValue.sum !== undefined) {
          responseValue = responseValue.sum;
        }
      }

      function sanitize (value, figure) {
        if (value < 0) {
          value = 0;
        }
        else {
          if (figure.units === 'bytes') {
            value /= (1024 * 1024);
          }
          value = Math.round(value * 100) / 100;
        }

        return value;
      }

      var newValue = 0;

      if (figure.type === 'current') {
        newValue = responseValue;
      }
      else {
        var n = self.seriesData[identifier].values.length;

        if (responseValue !== undefined && n > 0) {
          var previous = self.seriesData[identifier].values[n - 1];
          if (previous.value !== undefined) {
            newValue = responseValue - previous.value;
          }
        }
      }

      newValue = sanitize(newValue, figure);
      if (newValue > self.seriesData[identifier].max) {
        self.seriesData[identifier].max = newValue;
      }

      self.seriesData[identifier].values.push({ x: timeStamp, y: newValue, value: responseValue });
    });
  },

  saveGraphState: function () {
    var self = this;
    $.each(this.options.description.models[0].attributes.figures, function(k,v) {
      var identifier = v.identifier;
      var toCheck = $('#'+identifier+'Checkbox').is(':checked');
      if (toCheck === true) {
        self.graphState[identifier] = true;
      }
      else {
        self.graphState[identifier] = false;
      }
    });
    localStorage.setItem("dbGraphState", this.graphState);
  },

  loadGraphState: function () {
    localStorage.getItem("dbGraphState");
    $.each(this.graphState, function(k,v) {
      if (v === true) {
        $("#"+k).show();
      }
      else {
        $("#"+k).hide();
      }
    });
  },

  renderFigure: function (figure) {
    $('#' + figure.group).append(
      '<li class="statClient" id="' + figure.identifier + '">' +
      '<div class="boxHeader"><h6 class="dashboardH6">' + figure.name +
      '</h6>'+
      '<i class="icon-remove db-hide" value="'+figure.identifier+'"></i>' +
      '<i class="icon-info-sign db-info" value="'+figure.identifier+
      '" title="'+figure.description+'"></i>' +
      '<i class="icon-zoom-in db-zoom" value="'+figure.identifier+'"></i>' +
      '</div>' +
      '<div class="statChart" id="' + figure.identifier + 'Chart"><svg class="svgClass"/></div>' +
      '</li>'
    );

    console.log(figure.group);
    $('#' + figure.group + 'Divider').after(
      '<li><a><label class="checkbox checkboxLabel">'+
      '<input class="css-checkbox" type="checkbox" id='+figure.identifier+'Checkbox checked/>'+
      '<label class="css-label"/>' +
      figure.name + '</label></a></li>'
    );
    $('.db-info').tooltip({
      placement: "top"
    });
  }

});
