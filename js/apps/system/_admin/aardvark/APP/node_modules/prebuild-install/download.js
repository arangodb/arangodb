var path = require('path')
var fs = require('fs')
var get = require('simple-get')
var pump = require('pump')
var tfs = require('tar-fs')
var extend = require('xtend')
var noop = extend({
  http: function () {},
  silly: function () {}
}, require('noop-logger'))
var zlib = require('zlib')
var util = require('./util')
var error = require('./error')
var url = require('url')
var tunnel = require('tunnel-agent')
var mkdirp = require('mkdirp')

function downloadPrebuild (opts, cb) {
  var downloadUrl = util.getDownloadUrl(opts)
  var cachedPrebuild = util.cachedPrebuild(downloadUrl)
  var localPrebuild = util.localPrebuild(downloadUrl)
  var tempFile = util.tempFile(cachedPrebuild)

  var log = opts.log || noop

  if (opts.nolocal) return download()

  log.info('looking for local prebuild @', localPrebuild)
  fs.exists(localPrebuild, function (exists) {
    if (exists) {
      log.info('found local prebuild')
      cachedPrebuild = localPrebuild
      return unpack()
    }

    download()
  })

  function download () {
    ensureNpmCacheDir(function (err) {
      if (err) return onerror(err)

      log.info('looking for cached prebuild @', cachedPrebuild)
      fs.exists(cachedPrebuild, function (exists) {
        if (exists) {
          log.info('found cached prebuild')
          return unpack()
        }

        log.http('request', 'GET ' + downloadUrl)
        var reqOpts = { url: downloadUrl }
        var proxy = opts['https-proxy'] || opts.proxy

        if (proxy) {
          var parsedDownloadUrl = url.parse(downloadUrl)
          var parsedProxy = url.parse(proxy)
          var uriProtocol = (parsedDownloadUrl.protocol === 'https:' ? 'https' : 'http')
          var proxyProtocol = (parsedProxy.protocol === 'https:' ? 'Https' : 'Http')
          var tunnelFnName = [uriProtocol, proxyProtocol].join('Over')
          reqOpts.agent = tunnel[tunnelFnName]({
            proxy: {
              host: parsedProxy.hostname,
              port: +parsedProxy.port,
              proxyAuth: parsedProxy.auth
            }
          })
          log.http('request', 'Proxy setup detected (Host: ' +
            parsedProxy.hostname + ', Port: ' +
            parsedProxy.port + ', Authentication: ' +
            (parsedProxy.auth ? 'Yes' : 'No') + ')' +
            ' Tunneling with ' + tunnelFnName)
        }

        var req = get(reqOpts, function (err, res) {
          if (err) return onerror(err)
          log.http(res.statusCode, downloadUrl)
          if (res.statusCode !== 200) return onerror()
          mkdirp(util.prebuildCache(), function () {
            log.info('downloading to @', tempFile)
            pump(res, fs.createWriteStream(tempFile), function (err) {
              if (err) return onerror(err)
              fs.rename(tempFile, cachedPrebuild, function (err) {
                if (err) return cb(err)
                log.info('renaming to @', cachedPrebuild)
                unpack()
              })
            })
          })
        })

        req.setTimeout(30 * 1000, function () {
          req.abort()
        })
      })

      function onerror (err) {
        fs.unlink(tempFile, function () {
          cb(err || error.noPrebuilts(opts))
        })
      }
    })
  }

  function unpack () {
    var binaryName

    var updateName = opts.updateName || function (entry) {
      if (/\.node$/i.test(entry.name)) binaryName = entry.name
    }

    log.info('unpacking @', cachedPrebuild)

    var options = {
      readable: true,
      writable: true,
      hardlinkAsFilesFallback: true
    }
    var extract = tfs.extract(opts.path, options).on('entry', updateName)

    pump(fs.createReadStream(cachedPrebuild), zlib.createGunzip(), extract,
    function (err) {
      if (err) return cb(err)

      var resolved
      if (binaryName) {
        try {
          resolved = path.resolve(opts.path || '.', binaryName)
        } catch (err) {
          return cb(err)
        }
        log.info('unpack', 'resolved to ' + resolved)

        if (opts.platform === process.platform && opts.abi === process.versions.modules) {
          try {
            require(resolved)
          } catch (err) {
            return cb(err)
          }
          log.info('unpack', 'required ' + resolved + ' successfully')
        }
      }

      cb(null, resolved)
    })
  }

  function ensureNpmCacheDir (cb) {
    var cacheFolder = util.npmCache()
    if (fs.access) {
      fs.access(cacheFolder, fs.R_OK | fs.W_OK, function (err) {
        if (err && err.code === 'ENOENT') {
          return makeNpmCacheDir()
        }
        cb(err)
      })
    } else {
      fs.exists(cacheFolder, function (exists) {
        if (!exists) return makeNpmCacheDir()
        cb()
      })
    }

    function makeNpmCacheDir () {
      log.info('npm cache directory missing, creating it...')
      mkdirp(cacheFolder, cb)
    }
  }
}

module.exports = downloadPrebuild
