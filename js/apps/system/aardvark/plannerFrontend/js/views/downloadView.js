/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, plannerTemplateEngine, alert */

(function() {
  "use strict";
  
  window.DownloadView = Backbone.View.extend({
    el: "#content",
    template: templateEngine.createTemplate("download.ejs"),

    events: {
      "click #startDownload": "download"
    },

    download: function() {
      window.open(this.content, "clusterConfiguration.json");
    },

    render: function(content) {
      this.content = "data:application/octet-stream," + encodeURIComponent(JSON.stringify(content, 2));
      var toShow = _.findWhere(content.runInfo.runInfo, {isStartServers: true});
      $(this.el).html(this.template.render({
        endpoints: _.map(toShow.endpoints, function(e) {
          return e.replace("tcp://","http://").replace("ssl://", "https://");
        }),
        roles: toShow.roles
      }));
    }
  });

}());

