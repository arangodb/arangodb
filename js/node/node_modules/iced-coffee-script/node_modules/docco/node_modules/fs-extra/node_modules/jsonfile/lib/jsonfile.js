var fs = require('fs');

var me = module.exports;

me.spaces = 2;

me.readFile = function(file, callback) {
  fs.readFile(file, 'utf8', function(err, data) {
    if (err) return callback(err, null);

    var obj = null;
    try {
      obj = JSON.parse(data);
    } catch (err2) {
      callback(err2, null);
      return;
    }
    callback(null, obj);
  })
}

me.readFileSync = function(file) {
  return JSON.parse(fs.readFileSync(file, 'utf8'));
}

me.writeFile = function(file, obj, options, callback) {
  if (callback == null) { // odd little swap because options is optional
    callback = options;
    options = null;
  }

  var str = '';
  try {
    str = JSON.stringify(obj, null, module.exports.spaces);
  } catch (err) {
    if (callback) {
      callback(err, null);
    }
    return;
  }
  fs.writeFile(file, str, options, callback);
}

me.writeFileSync = function(file, obj, options) {
  var str = JSON.stringify(obj, null, module.exports.spaces);
  return fs.writeFileSync(file, str, options); //not sure if fs.writeFileSync returns anything, but just in case
}