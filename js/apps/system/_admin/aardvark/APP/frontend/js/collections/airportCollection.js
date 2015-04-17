/*jshint browser: true */
/*jshint unused: false */
/*global window, $, _ */
(function () {

  "use strict";

  window.Airports = Backbone.Collection.extend({

    initialize: function() {
    },

    getAirports: function(callback) {
      var self = this;

      $.ajax({
        type: "POST",
        url: "/_api/cursor",
        data: JSON.stringify({
          query: "for a in airports return a"
        }),
        contentType: "application/json",
        processData: false,
        success: function (data) {
          _.each(data.result, function(airport) {
            self.add({
              "Airport ID": airport["Airport ID"],
              Latitude: airport.Latitude,
              Longitude: airport.Longitude,
              Altitude: airport.Altitude,
              Timezone: airport.Timezon,
              Name: airport.Name,
              City: airport.City,
              Country: airport.Country,
              ICAO: airport.ICAO,
              DST: airport.DST,
              "Tz time zone": airport["Tz time zone"]
            });
          });
          if (callback) {
            callback();
          }
        },
        error: function (data) {
        }
      });
    },

    model: window.Airport

  });
}());
