

const mime = 'image/gif';
const pixelGif = new Buffer('R0lGODlhAQABAIAAAP///wAAACH5BAEAAAAALAAAAAABAAEAAAICRAEAOw==', 'base64');
const createRouter = require('@arangodb/foxx/router');
const ArangoError = require('@arangodb').ArangoError;
const errors = require('internal').errors;
const router = createRouter();

router.post('/interface-echo', function (req, res) {
  print(JSON.stringify(req))
  res.json({
    req: req.headers,
    rawPostbody: req._raw.requestBody
  });
});

router.put('/interface-echo', function (req, res) {
  res.json(req.headers);
});

router.delete('/interface-echo', function (req, res) {
  res.json(req.headers);
});

router.patch('/interface-echo', function (req, res) {
  res.json(req.headers);
});

router.head('/interface-echo', function (req, res) {
  res.json(req.headers);
});

router.get('/interface-echo', function (req, res) {
  res.json(req.headers);
});

router.post('/interface-echo-bin', function (req, res) {
  print(pixelGif)
  print(req._raw.requestBody)
  let postBuffer = new Buffer(req._raw.requestBody);
  print(postBuffer)
  if (postBuffer !== pixelGif) {
    throw new ArangoError({
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: "pixel gifs don't match"
    });
  }
  print(JSON.stringify(req))
  res.json({
    req: req.headers,
    rawPostbody: req._raw.requestBody
  });
});

router.put('/interface-echo-bin', function (req, res) {
  res.json(req.headers);
});

router.delete('/interface-echo-bin', function (req, res) {
  res.json(req.headers);
});

router.patch('/interface-echo-bin', function (req, res) {
  res.json(req.headers);
});

router.head('/interface-echo-bin', function (req, res) {
  res.json(req.headers);
});

router.get('/interface-echo-bin', function (req, res) {
  res.json(req.headers);
});

module.context.use(router);
