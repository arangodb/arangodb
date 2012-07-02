################################################################################
### @brief error handling
###
### @file
###
### DISCLAIMER
###
### Copyright 2012 triagens GmbH, Cologne, Germany
###
### Licensed under the Apache License, Version 2.0 (the "License");
### you may not use this file except in compliance with the License.
### You may obtain a copy of the License at
###
###     http://www.apache.org/licenses/LICENSE-2.0
###
### Unless required by applicable law or agreed to in writing, software
### distributed under the License is distributed on an "AS IS" BASIS,
### WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
### See the License for the specific language governing permissions and
### limitations under the License.
###
### Copyright holder is triAGENS GmbH, Cologne, Germany
###
### @author Dr. Frank Celler
### @author Copyright 2012, triAGENS GmbH, Cologne, Germany
################################################################################

module Arango

## -----------------------------------------------------------------------------
## --SECTION--                                                       HttpRequest
## -----------------------------------------------------------------------------

  class HttpRequest
    def body()
      return @body
    end

    def headers()
      return @headers
    end

    def parameters()
      return @parameters
    end

    def request_type()
      return @request_type
    end

    def suffix()
      return @suffix
    end
  end

## -----------------------------------------------------------------------------
## --SECTION--                                                      HttpResponse
## -----------------------------------------------------------------------------

  class HttpResponse
    def content_type()
      return @content_type
    end

    def content_type=(type)
      @content_type = type
    end

    def body()
      return @body
    end

    def body=(text)
      @body = text.to_s
    end
    def status()
      return @status
    end

    def status=(code)
      @status = code.to_i
    end
  end

## -----------------------------------------------------------------------------
## --SECTION--                                                   AbstractServlet
## -----------------------------------------------------------------------------

  class AbstractServlet
    @@HTTP_OK                   = 200
    @@HTTP_CREATED              = 201
    @@HTTP_ACCEPTED             = 202
    @@HTTP_PARTIAL              = 203
    @@HTTP_NO_CONTENT           = 204

    @@HTTP_MOVED_PERMANENTLY    = 301
    @@HTTP_FOUND                = 302
    @@HTTP_SEE_OTHER            = 303
    @@HTTP_NOT_MODIFIED         = 304
    @@HTTP_TEMPORARY_REDIRECT   = 307

    @@HTTP_BAD                  = 400
    @@HTTP_UNAUTHORIZED         = 401
    @@HTTP_PAYMENT              = 402
    @@HTTP_FORBIDDEN            = 403
    @@HTTP_NOT_FOUND            = 404
    @@HTTP_METHOD_NOT_ALLOWED   = 405
    @@HTTP_CONFLICT             = 409
    @@HTTP_PRECONDITION_FAILED  = 412
    @@HTTP_UNPROCESSABLE_ENTITY = 422

    @@HTTP_SERVER_ERROR         = 500
    @@HTTP_NOT_IMPLEMENTED      = 501
    @@HTTP_BAD_GATEWAY          = 502
    @@HTTP_SERVICE_UNAVAILABLE  = 503

    def service(req, res)
      p "Body: <#{req.body}>"
      p "Headers: <#{req.headers}>"
      p "Parameters: <#{req.parameters}>"
      p "RequestType: <#{req.request_type}>"
      p "Suffix: <#{req.suffix}>"

      method = req.request_type

      if method == "GET"
	self.do_GET(req, res)
      elsif method == "PUT"
	self.do_PUT(req, res)
      elsif method == "POST"
	self.do_POST(req, res)
      elsif method == "DELETE"
	self.do_DELETE(req, res)
      elsif method == "HEAD"
	self.do_HEAD(req, res)
      else
	generate_unknown_method(req, res, method)
      end
    end

    def do_GET(req, res)
      res.status = @@HTTP_METHOD_NOT_ALLOWED
    end

    def do_PUT(req, res)
      res.status = @@HTTP_METHOD_NOT_ALLOWED
    end

    def do_POST(req, res)
      res.status = @@HTTP_METHOD_NOT_ALLOWED
    end

    def do_DELETE(req, res)
      res.status = @@HTTP_METHOD_NOT_ALLOWED
    end

    def do_HEAD(req, res)
      res.status = @@HTTP_METHOD_NOT_ALLOWED
    end

    def generate_unknown_method(req, res, method)
      res.status = @@HTTP_METHOD_NOT_ALLOWED
    end
  end

end

## -----------------------------------------------------------------------------
## --SECTION--                                                       END-OF-FILE
## -----------------------------------------------------------------------------

## Local Variables:
## mode: outline-minor
## outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
## End:
