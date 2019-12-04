var path = require('path')
var github = require('github-from-package')
var home = require('os-homedir')
var expandTemplate = require('expand-template')()

function getDownloadUrl (opts) {
  var pkgName = opts.pkg.name.replace(/^@\w+\//, '')
  return expandTemplate(urlTemplate(opts), {
    name: pkgName,
    package_name: pkgName,
    version: opts.pkg.version,
    major: opts.pkg.version.split('.')[0],
    minor: opts.pkg.version.split('.')[1],
    patch: opts.pkg.version.split('.')[2],
    prerelease: opts.pkg.version.split('-')[1],
    build: opts.pkg.version.split('+')[1],
    abi: opts.abi || process.versions.modules,
    node_abi: process.versions.modules,
    runtime: opts.runtime || 'node',
    platform: opts.platform,
    arch: opts.arch,
    libc: opts.libc || process.env.LIBC || '',
    configuration: (opts.debug ? 'Debug' : 'Release'),
    module_name: opts.pkg.binary && opts.pkg.binary.module_name
  })
}

function urlTemplate (opts) {
  if (typeof opts.download === 'string') {
    return opts.download
  }

  var packageName = '{name}-v{version}-{runtime}-v{abi}-{platform}{libc}-{arch}.tar.gz'
  var hostMirrorUrl = getHostMirrorUrl(opts)

  if (hostMirrorUrl) {
    return hostMirrorUrl + '/v{version}/' + packageName
  }

  if (opts.pkg.binary) {
    return [
      opts.pkg.binary.host,
      opts.pkg.binary.remote_path,
      opts.pkg.binary.package_name || packageName
    ].map(function (path) {
      return trimSlashes(path)
    }).filter(Boolean).join('/')
  }

  return github(opts.pkg) + '/releases/download/v{version}/' + packageName
}

function getHostMirrorUrl (opts) {
  var propName = 'npm_config_' + opts.pkg.name + '_binary_host'
  return process.env[propName] || process.env[propName + '_mirror']
}

function trimSlashes (str) {
  if (str) return str.replace(/^\.\/|^\/|\/$/g, '')
}

function cachedPrebuild (url) {
  return path.join(prebuildCache(), url.replace(/[^a-zA-Z0-9.]+/g, '-'))
}

function npmCache () {
  var env = process.env
  return env.npm_config_cache || (env.APPDATA ? path.join(env.APPDATA, 'npm-cache') : path.join(home(), '.npm'))
}

function prebuildCache () {
  return path.join(npmCache(), '_prebuilds')
}

function tempFile (cached) {
  return cached + '.' + process.pid + '-' + Math.random().toString(16).slice(2) + '.tmp'
}

function localPrebuild (url) {
  return path.join('prebuilds', path.basename(url))
}

function isYarnPath (execPath) {
  return execPath ? /^yarn/.test(path.basename(execPath)) : false
}

exports.getDownloadUrl = getDownloadUrl
exports.urlTemplate = urlTemplate
exports.cachedPrebuild = cachedPrebuild
exports.localPrebuild = localPrebuild
exports.prebuildCache = prebuildCache
exports.npmCache = npmCache
exports.tempFile = tempFile
exports.isYarnPath = isYarnPath
