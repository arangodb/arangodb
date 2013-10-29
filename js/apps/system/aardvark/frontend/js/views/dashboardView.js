/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, flush, window, arangoHelper, nv, d3, localStorage*/

var dashboardView = Backbone.View.extend({
  el: '#content',
  updateInterval: 1000, // 1 second, constant
  updateFrequency: 5, // the actual update rate (5 s)
  updateFrequencyR: 5, // the actual update rate (5 s) REPLICATION
  updateCounter: 0,
  updateCounterR: 0,
  arraySize: 20, // how many values will we keep per figure?
  seriesData: {},
  charts: {},
  units: [],
  graphState: {},
  updateNOW: false,
  detailGraph: "userTime",

  initialize: function () {
    this.loadGraphState();
    this.arangoReplication = new window.ArangoReplication();
    var self = this;

    this.initUnits();

    this.collection.fetch({
      success: function() {
        self.addCustomCharts();
        self.calculateSeries();
        self.renderCharts();

        window.setInterval(function() {
          self.updateCounter++;
          self.updateCounterR++;

          if (self.updateNOW === true) {
            self.calculateSeries();
            self.renderCharts();
            self.updateNOW = false;
          }

          if (window.location.hash === '' && self.updateCounterR > self.updateFrequencyR) {
            self.getReplicationStatus();
            self.updateCounterR = 0;
          }

          if (self.updateCounter < self.updateFrequency) {
            return false;
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
              self.renderCharts();
            }
          });

        }, self.updateInterval);
      }
    });
  },

  events: {
    "click #dashboardDropdown li"  : "checkEnabled",
    "click #intervalUL li"         : "checkInterval",
    "click #intervalULR li"        : "checkIntervalR",
    "click .db-zoom"               : "renderDetailChart",
    "click .db-minimize"           : "checkDetailChart",
    "click .db-hide"               : "hideChart",
    "click .group-close"           : "hideGroup",
    "click .group-open"            : "showGroup",
    "click .dashSwitch"            : "showCategory",
    "click #dashboardToggle"       : "toggleEvent"
  },

  template: new EJS({url: 'js/templates/dashboardView.ejs'}),

  toggleEvent: function () {
    if ($('#detailReplication').is(':visible')) {
      $('#replicationDropdownOut').slideToggle(220);
    }
    else {
      $('#dashboardDropdownOut').slideToggle(220);
    }
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
    //var runningLog = '<div class="' + cls + 'Class">' + cls + '</div>';
    var runningLog = '-';

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
    else {
      lastLog = this.replLogState.state.lastLogTick;
    }

    var numEvents = this.replLogState.state.totalEvents || 0;

    //log table
    if (cls === true) {
      runningLog = '<div class="greenLight"/>';
    }
    else {
      runningLog = '<div class="redLight"/>';
    }

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
    //var runningApply = '<div class="' + cls + 'Class">' + cls + '</div>';
    var runningApply = '-';

    numEvents = this.replApplyState.state.totalEvents || 0;
    numRequests = this.replApplyState.state.totalRequests || 0;
    numFailed = this.replApplyState.state.totalFailedConnects || 0;

    if (cls === true) {
      runningApply = '<div class="greenLight"/>';
    }
    else {
      runningApply = '<div class="redLight"/>';
    }

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
    
    arangoHelper.fixTooltips(".glyphicon", "top");

    var counter = 1;

    $.each(this.options.description.models[0].attributes.groups, function () {
      $('#dbThumbnailsIn').append(
        '<ul class="statGroups" id="' + this.group + '">' +
        '<i class="group-close icon-minus icon-white"></i>' +
        '<div id="statsHeaderDiv"><h4 class="statsHeader">' + this.name + '</h4></div>' +
        '</ul>');

      //group
      $('#dashboardDropdown').append('<ul id="' + this.group + 'Ul"></ul>');
      $('#'+this.group+'Ul').append('<li class="nav-header">' + this.name + '</li>');

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
    $('#every'+self.updateFrequencyR+'secondsR').prop('checked',true);

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
      "description" : "Cumulated values of User + System Time",
      "group" : "system",
      "identifier" : "userSystemTime",
      "name" : "User + System Time",
      "type" : "accumulated",
      "units" : "seconds",
      "exec" : function () {
        var val1 = self.collection.models[0].attributes.system.userTime;
        var val2 = self.collection.models[0].attributes.system.systemTime;

        if (val1 === undefined || val2 === undefined) {
          return undefined;
        }
        return val1 + val2;
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

    this.options.description.models[0].attributes.figures.unshift(figure);
  },

  checkInterval: function (a) {
    this.updateFrequency = a.target.value;
    this.calculateSeries();
    this.renderCharts();
  },
  checkIntervalR: function (a) {
    if (a.target.value) {
      this.updateFrequencyR = a.target.value;
      this.getReplicationStatus();
    }
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

  checkMaxValue: function (identifier) {
    var maxValue = 0;
    $.each(this.seriesData[identifier].values, function (k,v) {
      if (v.y > maxValue) {
        maxValue = v.y;
      }
    });
    this.seriesData[identifier].max = maxValue;
  },

  getMaxValue : function (identifier) {
    this.checkMaxValue(identifier);
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
    var parent = $(e.target).parent().attr('id');
    $('.arangoTab li').removeClass('active');
    $('.arangoTab #'+parent).addClass('active');
    if (parent === 'replSwitch') {
      $('#detailGraph').hide();
      $('.statGroups').hide();
        $('#detailReplication').show();
      if ($('#dashboardDropdown').is(':visible') || $('#replicationDropdown').is(':visible')) {
        $('#replicationDropdownOut').show();
        $('#dashboardDropdownOut').hide();
      }
    }
    else if (parent === 'statSwitch') {
      $('#detailReplication').hide();
      $('#detailGraph').show();
      $('.statGroups').show();
      if ($('#dashboardDropdown').is(':visible') || $('#replicationDropdown').is(':visible')) {
        $('#replicationDropdownOut').hide();
        $('#dashboardDropdownOut').show();
      }
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
        $('#detailGraph').height(400);
        $('#dbHideSwitch').addClass('icon-minus');
        $('#dbHideSwitch').removeClass('icon-plus');
        self.updateNOW = true;
        self.calculateSeries();
        self.renderCharts();
      }
    });
  },

  renderCharts: function () {
    var self = this;
    $('#every'+self.updateFrequency+'seconds').prop('checked',true);
    $('#every'+self.updateFrequencyR+'secondsR').prop('checked',true);

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

	chart.tooltips(false);

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

    //fix position for last x-value label in detailgraph
    $('.nv-x.nv-axis .nvd3.nv-wrap.nv-axis:last-child text').attr('x','-5');
    //fix position of small graphs
    $('.svgClass .nv-lineChart').attr('transform','translate(5,5)');
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
    var loadGraphState = localStorage.getItem("dbGraphState");
    if (loadGraphState === undefined) {
      return;
    }
    $.each(this.graphState, function(k,v) {
      if (v === true) {
        $("#"+k).show();
        $('#'+k+'Checkbox').prop('checked', true);
      }
      else {
        $("#"+k).hide();
        $('#'+k+'Checkbox').prop('checked', false);
      }
    });
  },

  renderFigure: function (figure) {
    $('#' + figure.group).append(
      '<li class="statClient" id="' + figure.identifier + '">' +
      '<div class="boxHeader"><h6 class="dashboardH6">' + figure.name +
      '</h6>'+
      '<i class="icon-remove db-hide" value="' + figure.identifier + '"></i>' +
      '<i class="icon-info-sign db-info" value="' + figure.identifier +
      '" title="' + figure.description + '"></i>' +
      '<i class="icon-zoom-in db-zoom" value="' + figure.identifier + '"></i>' +
      '</div>' +
      '<div class="statChart" id="' + figure.identifier + 'Chart"><svg class="svgClass"/></div>' +
      '</li>'
    );

    $('#' + figure.group + 'Ul').append(
      '<li><a><label class="checkbox checkboxLabel">'+
      '<input class="css-checkbox" type="checkbox" id="'+figure.identifier+'Checkbox" checked/>'+
      '<label class="css-label"/>' +
      figure.name + '</label></a></li>'
    );
    //tooltips small charts
    arangoHelper.fixTooltips(".db-info", "top");
  }

});
