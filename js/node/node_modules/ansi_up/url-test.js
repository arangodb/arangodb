
var AU = require('./ansi_up.js')
var ansi_up = new AU.default;

var txt = "ABC \x1b]8;;http://example.com\x1b\\EXAMPLE\x1b]8;;\x07 DEF"

var res = ansi_up.ansi_to_html(txt)

console.log(JSON.stringify(res));

console.log(JSON.stringify(ansi_up._buffer));

