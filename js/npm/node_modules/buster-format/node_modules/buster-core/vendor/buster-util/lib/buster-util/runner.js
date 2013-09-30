var fs = require("fs");
var path = require("path");

module.exports = function testDir(dir, filter) {
    fs.readdir(dir, function (err, files) {
        if (!files) {
            return;
        }

        files.forEach(function (file) {
            var fullPath = path.join(dir, file);

            fs.stat(fullPath, function (err, stat) {
                if (!stat) {
                    return;
                }

                if (stat.isDirectory()) {
                    testDir(fullPath, filter);
                } else {
                    if (/\.js$/.test(file) && !/browser/.test(fullPath) &&
                        (!filter || filter.test(fullPath))) {
                        require(fullPath.replace(/\.js$/, ""));
                    }
                }
            });
        });
    });
};
