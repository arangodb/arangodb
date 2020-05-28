/* jshint unused: false */
/* global $, window, _, arangoHelper */
(function () {
  'use strict';

  var showInterface = function (currentVersion, json) {
    var buttons = [];

    buttons.push(window.modalView.createSuccessButton('Download Page', function () {
      window.open('https://www.arangodb.com/download', '_blank');
      window.modalView.hide();
    }));

    var infos = [];
    var cEntry = window.modalView.createReadOnlyEntry.bind(window.modalView);
    infos.push(cEntry('current', 'Current', currentVersion.toString()));

    if (json.major) {
      infos.push(cEntry('major', 'Major', json.major.version));
    }
    if (json.minor) {
      infos.push(cEntry('minor', 'Minor', json.minor.version));
    }
    if (json.bugfix) {
      infos.push(cEntry('bugfix', 'Bugfix', json.bugfix.version));
    }

    window.modalView.show(
      'modalTable.ejs', 'New Version Available', buttons, infos
    );
  };

  window.checkVersion = function () {
    // this checks for version updates
    $.ajax({
      type: 'GET',
      cache: false,
      url: arangoHelper.databaseUrl('/_admin/status?overview=true'),
      contentType: 'application/json',
      processData: false,
      async: true,
      success: function (data) {
        var currentVersion =
        window.versionHelper.fromString(data.version);

        $('.navbar #currentVersion').html(
          data.version + '<i class="fa fa-check-circle"></i>'
        );

        window.parseVersions = function (json) {
          if (_.isEmpty(json)) {
            $('#currentVersion').addClass('up-to-date');
            return; // no new version.
          }
          $('#currentVersion').addClass('out-of-date');
          $('#currentVersion .fa').removeClass('fa-check-circle').addClass('fa-exclamation-circle');
          $('#currentVersion').click(function () {
            showInterface(currentVersion, json);
          });
        };

        $.ajax({
          type: 'GET',
          async: true,
          crossDomain: true,
          timeout: 3000,
          dataType: 'jsonp',
          url: 'https://www.arangodb.com/versions.php' +
            '?jsonp=parseVersions'
            + '&version=' + encodeURIComponent(data.version)
            + '&platform=' + encodeURIComponent(data.platform)
            + '&engine=' + encodeURIComponent(data.engine)
            + '&license=' + encodeURIComponent(data.license)
            + '&source=ui'
            + '&hash=' + encodeURIComponent(data.hash),
          error: function (e) {
            if (e.status === 200) {
              window.activeInternetConnection = true;
            } else {
              window.activeInternetConnection = false;
            }
          },
          success: function (e) {
            window.activeInternetConnection = true;
          }
        });
      }
    });
  };
}());
