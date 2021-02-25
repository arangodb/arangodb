
const pixelStr = 'R0lGODlhAQABAIAAAP///wAAACH5BAEAAAAALAAAAAABAAEAAAICRAEAOw==';
const pixelGif = new Buffer(pixelStr, 'base64');
const binaryMime = 'image/gif';
const textMime = 'text/plain';
const jsonMime = 'application/json; charset=utf-8';
const createRouter = require('@arangodb/foxx/router');
const ArangoError = require('@arangodb').ArangoError;
const errors = require('internal').errors;
const router = createRouter();

const cmpBuffer = function(a, b) {
  if (a.length !== b.length) {
    return false;
  }
  if (typeof b === 'string') {
    for (let i = 0; i < a.length; ++i) {
      if (a[i] !== b.charCodeAt(i)) {
        return false;
      }
    }
  } else {
    for (let i = 0; i < a.length; ++i) {
      if (a[i] !== b[i]) {
        return false;
      }
    }
  }
  return true;
};

router.post('/interface-echo', function (req, res) {
  let postBuffer = req._raw.requestBody;
  if (postBuffer !== JSON.stringify({pixelStr}) ) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: "pixel gifs don't match"
    });
  }
  if (req._raw.headers['content-type'] !== jsonMime) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: `mimetypes don't match - ${jsonMime} !== ${req._raw.headers.content-type}`
    });
  }
  let bufLen = postBuffer.length;
  let headerLen = parseInt(req._raw.headers['content-length']);
  if (headerLen !== bufLen) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: `content-length header doesn't match - ${bufLen} !== ${headerLen}`
    });
  }
  res.write({pixelStr});
  res.set('test', 'header');
  res.set('request-type', 'POST_RAW');
});

router.put('/interface-echo', function (req, res) {
  let postBuffer = req._raw.requestBody;
  if (postBuffer !== JSON.stringify({pixelStr}) ) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: "pixel gifs don't match"
    });
  }
  if (req._raw.headers['content-type'] !== jsonMime) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: `mimetypes don't match - ${jsonMime} !== ${req._raw.headers.content-type}`
    });
  }
  let bufLen = postBuffer.length;
  let headerLen = parseInt(req._raw.headers['content-length']);
  if (headerLen !== bufLen) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: `content-length header doesn't match - ${bufLen} !== ${headerLen}`
    });
  }
  res.write({pixelStr});
  res.set('test', 'header');
  res.set('request-type', 'PUT_RAW');
});

router.delete('/interface-echo', function (req, res){
  let postBuffer = req._raw.requestBody;
  if (postBuffer !== JSON.stringify({pixelStr}) ) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: "pixel gifs don't match"
    });
  }
  if (req._raw.headers['content-type'] !== jsonMime) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: `mimetypes don't match - ${jsonMime} !== ${req._raw.headers.content-type}`
    });
  }
  let bufLen = postBuffer.length;
  let headerLen = parseInt(req._raw.headers['content-length']);
  if (headerLen !== bufLen) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: `content-length header doesn't match - ${bufLen} !== ${headerLen}`
    });
  }
  res.write({pixelStr});
  res.set('test', 'header');
  res.set('request-type', 'DELETE_RAW');
}).body("I can haz body");

router.patch('/interface-echo', function (req, res) {
  let postBuffer = req._raw.requestBody;
  if (postBuffer !== JSON.stringify({pixelStr}) ) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: "pixel gifs don't match"
    });
  }
  if (req._raw.headers['content-type'] !== jsonMime) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: `mimetypes don't match - ${jsonMime} !== ${req._raw.headers.content-type}`
    });
  }
  let bufLen = postBuffer.length;
  let headerLen = parseInt(req._raw.headers['content-length']);
  if (headerLen !== bufLen) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: `content-length header doesn't match - ${bufLen} !== ${headerLen}`
    });
  }
  res.write({pixelStr});
  res.set('test', 'header');
  res.set('request-type', 'PATCH_RAW');
});

router.head('/interface-echo', function (req, res) {
  res.write({pixelStr});
  res.set('test', 'header');
  res.set('request-type', 'HEAD_RAW');
});

router.get('/interface-echo', function (req, res) {
  res.write({pixelStr});
  res.set('test', 'header');
  res.set('request-type', 'GET_RAW');
});

////////////////////////////////////////////////////////////////////////////////
//            Binary post body + reply
////////////////////////////////////////////////////////////////////////////////
router.post('/interface-echo-bin', function (req, res) {
  // convert SlowBuffer->Buffer
  let postBuffer = new Buffer(req._raw.rawRequestBody);
  if (!cmpBuffer(postBuffer, pixelGif)) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: "pixel gifs don't match"
    });
  }
  if (req._raw.headers['content-type'] !== binaryMime) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: `mimetypes don't match - ${binaryMime} !== ${req._raw.headers.content-type}`
    });
  }
  let bufLen = postBuffer.length;
  let headerLen = parseInt(req._raw.headers['content-length']);
  if (headerLen !== bufLen) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: `content-length header doesn't match - ${bufLen} !== ${headerLen}`
    });
  }
  res.write(pixelGif);
  res.set('content-type', binaryMime);
  res.set('test', 'header');
  res.set('request-type', 'POST_RAW');
});
router.put('/interface-echo-bin', function (req, res) {
  let postBuffer = new Buffer(req._raw.rawRequestBody);
  if (!cmpBuffer(postBuffer, pixelGif)) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: "pixel gifs don't match"
    });
  }
  if (req._raw.headers['content-type'] !== binaryMime) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: `mimetypes don't match - ${binaryMime} !== ${req._raw.headers['content-type']}`
    });
  }

  let bufLen = postBuffer.length;
  let headerLen = parseInt(req._raw.headers['content-length']);
  if (headerLen !== bufLen) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: `content-length header doesn't match - ${bufLen} !== ${headerLen}`
    });
  }
  res.write(pixelGif);
  res.set('content-type', binaryMime);
  res.set('test', 'header');
  res.set('request-type', 'PUT_RAW');
});

router.delete('/interface-echo-bin', function (req, res) {
  let postBuffer = new Buffer(req._raw.rawRequestBody);
  if (!cmpBuffer(postBuffer, pixelGif)) {
    res.throw({
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: "pixel gifs don't match"
    });
  }
  if (req._raw.headers['content-type'] !== binaryMime) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: `mimetypes don't match - ${binaryMime} !== ${req._raw.headers.content-type}`
    });
  }
  let bufLen = postBuffer.length;
  let headerLen = parseInt(req._raw.headers['content-length']);
  if (headerLen !== bufLen) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: `content-length header doesn't match - ${bufLen} !== ${headerLen}`
    });
  }
  res.write(pixelGif);
  res.set('content-type', binaryMime);
  res.set('test', 'header');
  res.set('request-type', 'DELETE_RAW');
}).body("I can haz body");

router.patch('/interface-echo-bin', function (req, res) {
  let postBuffer = new Buffer(req._raw.rawRequestBody);
  if (!cmpBuffer(postBuffer, pixelGif)) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: "pixel gifs don't match"
    });
  }
  if (req._raw.headers['content-type'] !== binaryMime) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: `mimetypes don't match - ${binaryMime} !== ${req._raw.headers.content-type}`
    });
  }
  let bufLen = postBuffer.length;
  let headerLen = parseInt(req._raw.headers['content-length']);
  if (headerLen !== bufLen) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: `content-length header doesn't match - ${bufLen} !== ${headerLen}`
    });
  }
  res.write(pixelGif);
  res.set('content-type', binaryMime);
  res.set('test', 'header');
  res.set('request-type', 'PATCH_RAW');
});

router.head('/interface-echo-bin', function (req, res) {
  res.write(pixelGif);
  res.set('content-type', binaryMime);
  res.set('test', 'header');
  res.set('request-type', 'HEAD_RAW');
});

router.get('/interface-echo-bin', function (req, res) {
  res.write(pixelGif);
  res.set('content-type', binaryMime);
  res.set('test', 'header');
  res.set('request-type', 'GET_RAW');
});

////////////////////////////////////////////////////////////////////////////////
//            text post body + reply
////////////////////////////////////////////////////////////////////////////////

router.post('/interface-echo-str', function (req, res) {
  let postBuffer = new Buffer(req._raw.rawRequestBody);
  if (!cmpBuffer(postBuffer, pixelStr)) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: "pixel gifs don't match"
    });
  }
  if (req._raw.headers['content-type'] !== textMime) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: `mimetypes don't match - ${textMime} !== ${req._raw.headers.content-type}`
    });
  }
  let bufLen = postBuffer.length;
  let headerLen = parseInt(req._raw.headers['content-length']);
  if (headerLen !== bufLen) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: `content-length header doesn't match - ${bufLen} !== ${headerLen}`
    });
  }
  res.write(pixelStr);
  res.set('content-type', textMime);
  res.set('test', 'header');
  res.set('request-type', 'POST_RAW');
});

router.put('/interface-echo-str', function (req, res) {
  let postBuffer = new Buffer(req._raw.rawRequestBody);
  if (!cmpBuffer(postBuffer, pixelStr)) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: "pixel gifs don't match"
    });
  }
  if (req._raw.headers['content-type'] !== textMime) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: `mimetypes don't match - ${textMime} !== ${req._raw.headers.content-type}`
    });
  }
  let bufLen = postBuffer.length;
  let headerLen = parseInt(req._raw.headers['content-length']);
  if (headerLen !== bufLen) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: `content-length header doesn't match - ${bufLen} !== ${headerLen}`
    });
  }
  res.write(pixelStr);
  res.set('content-type', textMime);
  res.set('test', 'header');
  res.set('request-type', 'PUT_RAW');
});

router.delete('/interface-echo-str', function (req, res) {
  let postBuffer = new Buffer(req._raw.rawRequestBody);
  if (!cmpBuffer(postBuffer, pixelStr)) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: "pixel gifs don't match"
    });
  }
  if (req._raw.headers['content-type'] !== textMime) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: `mimetypes don't match - ${textMime} !== ${req._raw.headers.content-type}`
    });
  }
  let bufLen = postBuffer.length;
  let headerLen = parseInt(req._raw.headers['content-length']);
  if (headerLen !== bufLen) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: `content-length header doesn't match - ${bufLen} !== ${headerLen}`
    });
  }
  res.write(pixelStr);
  res.set('content-type', textMime);
  res.set('test', 'header');
  res.set('request-type', 'DELETE_RAW');
}).body("I can haz body");

router.patch('/interface-echo-str', function (req, res) {
  let postBuffer = new Buffer(req._raw.rawRequestBody);
  if (!cmpBuffer(postBuffer, pixelStr)) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: "pixel gifs don't match"
    });
  }
  if (req._raw.headers['content-type'] !== textMime) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: `mimetypes don't match - ${textMime} !== ${req._raw.headers.content-type}`
    });
  }
  let bufLen = postBuffer.length;
  let headerLen = parseInt(req._raw.headers['content-length']);
  if (headerLen !== bufLen) {
    res.throw(500, {
      errorNum: errors.ERROR_VALIDATION_FAILED.code,
      errorMessage: `content-length header doesn't match - ${bufLen} !== ${headerLen}`
    });
  }
  res.write(pixelStr);
  res.set('content-type', textMime);
  res.set('test', 'header');
  res.set('request-type', 'PATCH_RAW');
});

router.head('/interface-echo-str', function (req, res) {
  res.write(pixelStr);
  res.set('content-type', textMime);
  res.set('test', 'header');
  res.set('request-type', 'HEAD_RAW');
});

router.get('/interface-echo-str', function (req, res) {
  res.write(pixelStr);
  res.set('content-type', textMime);
  res.set('test', 'header');
  res.set('request-type', 'GET_RAW');
});

module.context.use(router);
