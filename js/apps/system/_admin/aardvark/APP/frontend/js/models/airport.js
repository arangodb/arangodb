/*global window, Backbone */
(function() {
  "use strict";

  window.Airport = Backbone.Model.extend({
    defaults: {
      "Airport ID" : 0,
      "Latitude"   : 0,
      "Longitude"  : 0,
      "Altitude"   : 0,
      "Timezone"   : 0,
      "Name"       : "",
      "City"       : "",
      "Country"    : "",
      "ICAO"       : "",
      "DST"        : "",
      "Tz time zone" : ""
    }

  });
}());
