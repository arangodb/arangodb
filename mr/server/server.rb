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
## --SECTION--                                                     ArangoRequest
## -----------------------------------------------------------------------------

  class HttpRequest
    def body()
      return @body
    end

    def content_type()
      return @content_type
    end

    def status()
      return @status
    end
  end

## -----------------------------------------------------------------------------
## --SECTION--                                                    ArangoResponse
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

end

## -----------------------------------------------------------------------------
## --SECTION--                                                       END-OF-FILE
## -----------------------------------------------------------------------------

## Local Variables:
## mode: outline-minor
## outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
## End:
