/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, arangoHelper, $, window, templateEngine*/

(function () {
  "use strict";

  window.DemoView = Backbone.View.extend({

    //MAP SPECIFIC VARIABLES
    MAPicon: "M23.963,20.834L17.5,9.64c-0.825-1.429-2.175-1.429-3,0L8.037,20.834c-0.825,1.429-0.15,2.598,1.5,2.598h12.926C24.113,23.432,24.788,22.263,23.963,20.834z",
    MAPplaneicon: "M19.671,8.11l-2.777,2.777l-3.837-0.861c0.362-0.505,0.916-1.683,0.464-2.135c-0.518-0.517-1.979,0.278-2.305,0.604l-0.913,0.913L7.614,8.804l-2.021,2.021l2.232,1.061l-0.082,0.082l1.701,1.701l0.688-0.687l3.164,1.504L9.571,18.21H6.413l-1.137,1.138l3.6,0.948l1.83,1.83l0.947,3.598l1.137-1.137V21.43l3.725-3.725l1.504,3.164l-0.687,0.687l1.702,1.701l0.081-0.081l1.062,2.231l2.02-2.02l-0.604-2.689l0.912-0.912c0.326-0.326,1.121-1.789,0.604-2.306c-0.452-0.452-1.63,0.101-2.135,0.464l-0.861-3.838l2.777-2.777c0.947-0.947,3.599-4.862,2.62-5.839C24.533,4.512,20.618,7.163,19.671,8.11z",
    //ICON FROM http://raphaeljs.com/icons/#plane
    MAPcolor: "black",

    //QUERIES SECTION
    queries: [
      {
        name: "i am just a dummy name",
        value: "here is the place of the actual query statement"
      }
    ],

    el: '#content',

    initialize: function () {
      this.renderMap([]);

      var callback = function() {
        this.renderMap([]);
        console.log("airports loaded");
      }.bind(this);

      this.airportCollection = new window.Airports();
      this.airportCollection.getAirports(callback);
    },

    events: {
      "change #flightQuerySelect" : "runSelectedQuery"
    },

    template: templateEngine.createTemplate("demoView.ejs"),

    render: function () {
      $(this.el).html(this.template.render({}));
      this.renderAvailableQueries();

      return this;
    },

    renderAvailableQueries: function() {
      var position = 0;
      _.each(this.queries, function(query) {
        $('#flightQuerySelect').append('<option position="' + position + '">' + query.name + '<option>');
        position++;
      });
    },

    runSelectedQuery: function() {
      //TODO: RUN SELECTED QUERY IF USER SELECTS QUERY

      var currentQueryPos = $( "#flightQuerySelect option:selected" ).attr('position');
      var currentQuery = this.queries[parseInt(currentQueryPos)].value;

      console.log(currentQuery);
    },

    readCSV: function() {
    },

    parseToJSON: function() {
    },

    prepareData: function (data) {

      var self = this, imageData = [];

      //TODO: COUNTER EINGEBAUT DA BEI ALLEN EINTRÄGEN DER BROWSER EXPLODIERT
      var counter = 0;

      _.each(data.data, function(airport) {
        if (counter > 0) {
          imageData.push({
            latitude: airport.Latitude,
            longitude: airport.Longitude,
            type: "circle", // CIRCLE INSTEAD OF ICON
            //svgPath: self.MAPplaneicon, //ICON if wanted
            color: self.MAPcolor,
            scale: 0.2, //ICON SCALE
            //TODO: LABEL TEMP. DISABLED BECAUSE OF READABILITY
            //label: airport._key,
            labelShiftY:2
          });
        }

        counter++;
      });

      //render map when data parsing is complete
      this.renderMap(imageData);
    },

    renderMap: function(imageData) {

      var self = this;

      AmCharts.theme = AmCharts.themes.light;
      self.map = AmCharts.makeChart("demo-mapdiv", {
        type: "map",
        theme: "light",
        pathToImages: "img/ammap/",
        dataProvider: {
          map: "worldLow",
          images: imageData,
          getAreasFromMap: true
        },
        areaSettings: {
          autoZoom: true,
          selectedColor: self.MAPcolor
        }
      });
    }

  });
}());

