
exports.const = require './const'
mods = [
  require './runtime'
  require './library'
]

for mod in mods
  for k,v of mod
    exports[k] = v