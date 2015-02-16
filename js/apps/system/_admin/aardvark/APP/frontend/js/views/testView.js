/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, $, _, window, document, templateEngine, FileReader */

(function() {
  "use strict";

  window.testView = Backbone.View.extend({
    el: '#content',

    events: {
      "change #fileInput" : "readJSON"
    },

    template: templateEngine.createTemplate("testView.ejs"),

    readJSON: function() {
      var fileInput = document.getElementById('fileInput');
      var file = fileInput.files[0];
      var textType = 'application/json';

      if (file.type.match(textType)) {
        var reader = new FileReader();

        reader.onload = function(e) {
          $('#fileDisplayArea pre').text(reader.result);
        };

        reader.readAsText(file);
      }
      else {
        $('#fileDisplayArea pre').text("File not supported!");
      }
    },

    render: function() {
      $(this.el).html(this.template.render());
      return this;
    }
  });
}());
