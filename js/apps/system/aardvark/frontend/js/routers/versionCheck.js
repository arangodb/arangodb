/*jshint unused: false */
/*global $, window, _*/
(function() {
  "use strict";

  var disableVersionCheck = function() {
    $.ajax({
      type: "POST",
      url: "/_admin/aardvark/disableVersionCheck"
    });
  };

  var isVersionCheckEnabled = function(cb) {
    $.ajax({
      type: "GET",
      url: "/_admin/aardvark/shouldCheckVersion",
      success: function(data) {
        if (data === true) {
          cb();
        }
      }
    });
  };

  var showInterface = function(currentVersion, json) {
    var buttons = [];
    buttons.push(window.modalView.createNotificationButton("Don't ask again", function() {
      disableVersionCheck();
      window.modalView.hide();
    }));
    buttons.push(window.modalView.createSuccessButton("Download Page", function() {
      window.open('https://www.arangodb.com/download','_blank');
      window.modalView.hide();
    }));
    var infos = [];
    var cEntry = window.modalView.createReadOnlyEntry.bind(window.modalView);
    infos.push(cEntry("current", "Current", currentVersion.toString()));
    if (json.major) {
      infos.push(cEntry("major", "Major", json.major.version));
    }
    if (json.minor) {
      infos.push(cEntry("minor", "Minor", json.minor.version));
    }
    if (json.bugfix) {
      infos.push(cEntry("bugfix", "Bugfix", json.bugfix.version));
    }
    window.modalView.show(
      "modalTable.ejs", "New Version Available", buttons, infos
    );
  };

  window.checkVersion = function() {
    // this checks for version updates
    $.ajax({
      type: "GET",
      cache: false,
      url: "/_api/version",
      contentType: "application/json",
      processData: false,
      async: true,
      success: function(data) {
        var currentVersion =
          window.versionHelper.fromString(data.version);
        window.parseVersions = function (json) {
          if (_.isEmpty(json)) {
            return; // no new version.
          }
          if (/-devel$/.test(data.version)) {
            return; // ignore version in devel
          }
          isVersionCheckEnabled(showInterface.bind(window, currentVersion, json));
        };
        $.ajax({
          type: "GET",
          async: true,
          crossDomain: true,
          dataType: "jsonp",
          url: "https://www.arangodb.com/repositories/versions.php" +
          "?jsonp=parseVersions&version=" + encodeURIComponent(currentVersion.toString())
        });
      }
    });
  };
}());
