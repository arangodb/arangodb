
var fs = require('fs');
var path = require('path');

var HERE = __dirname;
var typings = path.resolve(HERE, '..', 'dist', 'ansi_up.d.ts');

var content = fs.readFileSync(typings, 'utf-8');

var new_content = content.replace(/^interface /mg, 'export interface ');
new_content = new_content.replace(/^declare class AnsiUp/mg, 'export default class AnsiUp');

if (new_content !== content) {
    fs.writeFileSync(typings, new_content, 'utf-8');
}
