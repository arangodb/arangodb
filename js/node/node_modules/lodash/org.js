'use strict';

var fs = require('fs-extra'),
    path = require('path');

var _ = require('lodash'),
    glob = require('glob'),
    moment = require('moment'),
    ncp = require('ncp').ncp;

var cwd = process.cwd();

var packages = _.transform(glob.sync(path.join(cwd, 'lodash.*')), function(result, pathname) {
  var stat = fs.statSync(path.join(pathname, 'index.js'));
  if (!moment(stat.mtime).isSame(stat.birthtime)) {
    result[path.basename(pathname)] = require(path.join(pathname, 'package.json'));
  }
}, {});

var grouped = _.groupBy(packages, 'version');

_.forOwn(grouped, function(packages, version) {
  var pathname = path.join(cwd, version);
  if (!fs.existsSync(pathname)) {
    fs.ensureDirSync(pathname);
  }
  _.each(packages, function(pkg) {
    var source = path.join(cwd, pkg.name),
        destination = path.join(pathname, pkg.name);

    ncp(source, destination, function (err) {
      if (err) {
        console.error(err);
        return;
      }
      // console.log('copied ' + pkg.name + ' to ' + path.join(version, pkg.name));
    });
  });
});
