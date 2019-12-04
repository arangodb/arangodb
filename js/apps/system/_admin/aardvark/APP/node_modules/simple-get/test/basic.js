var concat = require('concat-stream')
var http = require('http')
var get = require('../')
var selfSignedHttps = require('self-signed-https')
var str = require('string-to-stream')
var test = require('tape')
var zlib = require('zlib')

// Allow self-signed certs
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0'

test('simple get', function (t) {
  t.plan(4)

  var server = http.createServer(function (req, res) {
    t.equal(req.url, '/path')
    res.statusCode = 200
    res.end('response')
  })

  server.listen(0, function () {
    var port = server.address().port
    get('http://localhost:' + port + '/path', function (err, res) {
      t.error(err)
      t.equal(res.statusCode, 200)
      res.pipe(concat(function (data) {
        t.equal(data.toString(), 'response')
        server.close()
      }))
    })
  })
})

test('follow redirects (up to 10)', function (t) {
  t.plan(13)

  var num = 1
  var server = http.createServer(function (req, res) {
    t.equal(req.url, '/' + num, 'visited /' + num)
    num += 1

    if (num <= 10) {
      res.statusCode = 301
      res.setHeader('Location', '/' + num)
      res.end()
    } else {
      res.statusCode = 200
      res.end('response')
    }
  })

  server.listen(0, function () {
    var port = server.address().port
    get('http://localhost:' + port + '/1', function (err, res) {
      t.error(err)
      t.equal(res.statusCode, 200)
      res.pipe(concat(function (data) {
        t.equal(data.toString(), 'response')
        server.close()
      }))
    })
  })
})

test('do not follow redirects', function (t) {
  t.plan(2)

  var server = http.createServer(function (req, res) {
    t.equal(req.url, '/1', 'visited /1')

    res.statusCode = 301
    res.setHeader('Location', '/2')
    res.end()
  })

  server.listen(0, function () {
    var port = server.address().port
    get({
      url: 'http://localhost:' + port + '/1',
      maxRedirects: 0
    }, function (err) {
      t.ok(err instanceof Error, 'got error')
      server.close()
    })
  })
})

test('follow redirects (11 is too many)', function (t) {
  t.plan(11)

  var num = 1
  var server = http.createServer(function (req, res) {
    t.equal(req.url, '/' + num, 'visited /' + num)
    num += 1

    res.statusCode = 301
    res.setHeader('Location', '/' + num)
    res.end()
  })

  server.listen(0, function () {
    var port = server.address().port
    get('http://localhost:' + port + '/1', function (err) {
      t.ok(err instanceof Error, 'got error')
      server.close()
    })
  })
})

test('custom headers', function (t) {
  t.plan(2)

  var server = http.createServer(function (req, res) {
    t.equal(req.headers['custom-header'], 'custom-value')
    res.statusCode = 200
    res.end('response')
  })

  server.listen(0, function () {
    var port = server.address().port
    get({
      url: 'http://localhost:' + port,
      headers: {
        'custom-header': 'custom-value'
      }
    }, function (err, res) {
      t.error(err)
      res.resume()
      server.close()
    })
  })
})

test('gzip response', function (t) {
  t.plan(3)

  var server = http.createServer(function (req, res) {
    res.statusCode = 200
    res.setHeader('content-encoding', 'gzip')
    str('response').pipe(zlib.createGzip()).pipe(res)
  })

  server.listen(0, function () {
    var port = server.address().port
    get('http://localhost:' + port, function (err, res) {
      t.error(err)
      t.equal(res.statusCode, 200) // statusCode still works on gunzip stream
      res.pipe(concat(function (data) {
        t.equal(data.toString(), 'response')
        server.close()
      }))
    })
  })
})

test('deflate response', function (t) {
  t.plan(3)

  var server = http.createServer(function (req, res) {
    res.statusCode = 200
    res.setHeader('content-encoding', 'deflate')
    str('response').pipe(zlib.createDeflate()).pipe(res)
  })

  server.listen(0, function () {
    var port = server.address().port
    get('http://localhost:' + port, function (err, res) {
      t.error(err)
      t.equal(res.statusCode, 200) // statusCode still works on inflate stream
      res.pipe(concat(function (data) {
        t.equal(data.toString(), 'response')
        server.close()
      }))
    })
  })
})

test('https', function (t) {
  t.plan(4)

  var server = selfSignedHttps(function (req, res) {
    t.equal(req.url, '/path')
    res.statusCode = 200
    res.end('response')
  })

  server.listen(0, function () {
    var port = server.address().port
    get('https://localhost:' + port + '/path', function (err, res) {
      t.error(err)
      t.equal(res.statusCode, 200)
      res.pipe(concat(function (data) {
        t.equal(data.toString(), 'response')
        server.close()
      }))
    })
  })
})

test('redirect https to http', function (t) {
  t.plan(5)

  var httpPort = null
  var httpsPort = null

  var httpsServer = selfSignedHttps(function (req, res) {
    t.equal(req.url, '/path1')
    res.statusCode = 301
    res.setHeader('Location', 'http://localhost:' + httpPort + '/path2')
    res.end()
  })

  var httpServer = http.createServer(function (req, res) {
    t.equal(req.url, '/path2')
    res.statusCode = 200
    res.end('response')
  })

  httpsServer.listen(0, function () {
    httpsPort = httpsServer.address().port
    httpServer.listen(0, function () {
      httpPort = httpServer.address().port
      get('https://localhost:' + httpsPort + '/path1', function (err, res) {
        t.error(err)
        t.equal(res.statusCode, 200)
        res.pipe(concat(function (data) {
          t.equal(data.toString(), 'response')
          httpsServer.close()
          httpServer.close()
        }))
      })
    })
  })
})

test('redirect http to https', function (t) {
  t.plan(5)

  var httpsPort = null
  var httpPort = null

  var httpServer = http.createServer(function (req, res) {
    t.equal(req.url, '/path1')
    res.statusCode = 301
    res.setHeader('Location', 'https://localhost:' + httpsPort + '/path2')
    res.end()
  })

  var httpsServer = selfSignedHttps(function (req, res) {
    t.equal(req.url, '/path2')
    res.statusCode = 200
    res.end('response')
  })

  httpServer.listen(0, function () {
    httpPort = httpServer.address().port
    httpsServer.listen(0, function () {
      httpsPort = httpsServer.address().port
      get('http://localhost:' + httpPort + '/path1', function (err, res) {
        t.error(err)
        t.equal(res.statusCode, 200)
        res.pipe(concat(function (data) {
          t.equal(data.toString(), 'response')
          httpsServer.close()
          httpServer.close()
        }))
      })
    })
  })
})

test('post (text body)', function (t) {
  t.plan(4)

  var server = http.createServer(function (req, res) {
    t.equal(req.method, 'POST')
    res.statusCode = 200
    req.pipe(res)
  })

  server.listen(0, function () {
    var port = server.address().port
    var opts = {
      url: 'http://localhost:' + port,
      body: 'this is the body'
    }
    get.post(opts, function (err, res) {
      t.error(err)
      t.equal(res.statusCode, 200)
      res.pipe(concat(function (data) {
        t.equal(data.toString(), 'this is the body')
        server.close()
      }))
    })
  })
})

test('post (buffer body)', function (t) {
  t.plan(4)

  var server = http.createServer(function (req, res) {
    t.equal(req.method, 'POST')
    res.statusCode = 200
    req.pipe(res)
  })

  server.listen(0, function () {
    var port = server.address().port
    var opts = {
      url: 'http://localhost:' + port,
      body: new Buffer('this is the body')
    }
    get.post(opts, function (err, res) {
      t.error(err)
      t.equal(res.statusCode, 200)
      res.pipe(concat(function (data) {
        t.equal(data.toString(), 'this is the body')
        server.close()
      }))
    })
  })
})

test('get.concat', function (t) {
  t.plan(4)
  var server = http.createServer(function (req, res) {
    res.statusCode = 200
    res.end('blah blah blah')
  })

  server.listen(0, function () {
    var port = server.address().port
    get.concat('http://localhost:' + port, function (err, data, res) {
      t.error(err)
      t.equal(res.statusCode, 200)
      t.ok(Buffer.isBuffer(data), '`data` is type buffer')
      t.equal(data.toString(), 'blah blah blah')
      server.close()
    })
  })
})

test('access `req` object', function (t) {
  t.plan(2)

  var server = http.createServer(function (req, res) {
    res.statusCode = 200
    res.end('response')
  })

  server.listen(0, function () {
    var port = server.address().port
    var req = get('http://localhost:' + port, function (err, res) {
      t.error(err)
      res.resume() // discard data
      server.close()
    })

    req.on('socket', function () {
      t.pass('got `socket` event')
    })
  })
})
