var isNamedCallback = require('./is-named-callback')

function isCallingBack (node, exceptions) {
  var isCallExpression = node.type === 'CallExpression'
  var callee = node.callee || {}
  var nameIsCallback = isNamedCallback(callee.name, exceptions)
  var isCB = isCallExpression && nameIsCallback
  return isCB
}

module.exports = isCallingBack
