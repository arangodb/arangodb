# coding: utf-8

class ArangoMultipartBody

  @boundary = ""

  @parts = [ ]

################################################################################
## initialise a multipart message body
################################################################################

  def initialize (boundary = nil)
    @parts = [ ]
    if boundary.nil?
      @boundary = "XXXArangoBatchXXX"
    else
      @boundary = boundary
    end
  end

################################################################################
## get the message boundary
################################################################################

  def getBoundary
    return @boundary
  end

################################################################################
## add a part to a multipart message body
################################################################################

  def addPart (method, url, headers, body)
    part = { :method => method, :url => url, :headers => headers, :body => body }
    @parts.push(part)
  end

################################################################################
## get the string representation of a multipart message body
################################################################################

  def to_s 
    body = ""
    @parts.each do|part|
      # boundary
      body += "--" + @boundary + "\r\n"
      # header
      body += "Content-Type: application/x-arango-batchpart\r\n\r\n";

      # inner header
      body += part[:method] + " " + part[:url] + " HTTP/1.1\r\n"

      part[:headers].each do|key, value|
        body += key + ": " + value + "\r\n"
      end
      # body
      body += part[:body] + "\r\n"
    end 
    body += "--" + @boundary + "--\r\n"
  end

end
