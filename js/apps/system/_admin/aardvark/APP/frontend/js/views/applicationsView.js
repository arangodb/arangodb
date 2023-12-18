/* global templateEngine */
(function () {
  "use strict";

  window.ApplicationsView = Backbone.View.extend({
    installCallback: function (result) {
      var self = this;
      var errors = {
        ERROR_SERVICE_DOWNLOAD_FAILED: {
          code: 1752,
          message: "service download failed",
        },
      };

      if (result.error === false) {
        this.collection.fetch({
          success: function () {
            window.modalView.hide();
            arangoHelper.arangoNotification(
              "Services",
              "Service " + result.name + " installed."
            );
          },
        });
      } else {
        var res = result;
        if (result.hasOwnProperty("responseJSON")) {
          res = result.responseJSON;
        }
        switch (res.errorNum) {
          case errors.ERROR_SERVICE_DOWNLOAD_FAILED.code:
            arangoHelper.arangoError(
              "Services",
              "Unable to download application from the given repository."
            );
            break;
          default:
            arangoHelper.arangoError(
              "Services",
              res.errorNum + ". " + res.errorMessage
            );
        }
      }
    },
  });
})();
