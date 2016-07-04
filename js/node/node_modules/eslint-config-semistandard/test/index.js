var eslint = require('eslint')
var test = require('tape')
var path = require('path')

var linter = new eslint.CLIEngine({
  configFile: path.join(__dirname, '..', 'eslintrc.json')
})

test('api: lintText', function (t) {
  t.plan(1)
  var result = linter.executeOnText("console.log('hi there')\n\n")
  t.equals(result.results[0].messages[0].message, 'Missing semicolon.')
})
