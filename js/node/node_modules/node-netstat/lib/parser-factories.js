var normalizeValues = require('./utils').normalizeValues;

exports.linux = function (options) {
  options = options || {};
  var parseName = !!options.parseName;

  return function (line, callback) {
      var parts = line.split(/\s/).filter(String);
      if (!parts.length || parts[0].match(/^(tcp|udp)/) === null) {
          return;
      }

      // NOTE: insert null for missing state column on UDP
      if (parts[0].indexOf('udp') === 0) {
          parts.splice(5, 0, null);
      }

      var name = '';
      var pid = parts.slice(6, parts.length).join(" ");
      if (parseName && pid.indexOf('/') > 0) {
        var pidParts = pid.split('/');
        pid = pidParts[0];
        name = pidParts.slice(1, pidParts.length).join('/');
      }

      var item = {
          protocol: parts[0],
          local: parts[3],
          remote: parts[4],
          state: parts[5],
          pid: pid
      };

      if (parseName) {
        item.processName = name;
      }

      return callback(normalizeValues(item));
  };
};

exports.darwin = function (options) {
  options = options || {};

  return function (line, callback) {
      var parts = line.split(/\s/).filter(String);
      if (parts.length === 0 || (parts[0].indexOf('tcp') < 0 && parts[0].indexOf('udp') < 0)) {
          return;
      }

      const protocol = parts[0];

      var item = {
          protocol: protocol === 'tcp4' ? 'tcp' : protocol === 'udp4' ? 'udp' : protocol,
          local: parts[3],
          remote: parts[4],
          state: parts[5],
          pid: parts[protocol.indexOf('udp') === 0 ? 7 : 8]
      };

      return callback(normalizeValues(item));
  };
};

exports.win32 = function (options) {
  options = options || {};

  return function (line, callback) {
        var parts = line.split(/\s/).filter(String);
        if (!parts.length || (parts.length != 5 && parts.length != 4)) {
          return;
        };

        var item = {
            protocol: parts[0],
            local: parts[1],
            remote: parts[2],
            state: Number(parts[3]) ? null : parts[3],
            pid: parts[parts.length - 1]
        };

        return callback(normalizeValues(item));
    };
};
