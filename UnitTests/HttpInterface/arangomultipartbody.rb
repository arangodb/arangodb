# coding: utf-8

class ArangoMultipartBody

  @boundary = ""

  @parts = [ ]

################################################################################
## initialize a multipart message body
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

  def addPart (method, url, headers, body, contentId = "")
    part = { :method => method, :url => url, :headers => headers, :body => body, :contentId => contentId }
    @parts.push(part)
  end

################################################################################
## split the response body into its individual parts
################################################################################

  def getParts (boundary, body)
    parts = [ ]
    valid = false

    while 1
      position = body.index("--" + boundary)

      break if position != 0 or position == nil

      follow = body.slice(position + boundary.length + 2, 2)

      break if follow != "\r\n" and follow != "--"

      if follow == "--"
        valid = true
        break
      end if

      # strip boundary start from body
      body = body.slice(position + boundary.length + 4, body.length)
        
      # look for boundary end
      final = body.index("--" + boundary)

      break if final == nil

      # extract the part
      partTotal = body.slice(0, final)

      # look for envelope bounds
      position = partTotal.index("\r\n\r\n");
      break if position == nil


      r = Regexp.new('content-id: ([^\s]+)', Regexp::IGNORECASE)
       
      if r.match(partTotal.slice(0, position))
        contentId = Regexp.last_match(1)
      else
        contentId = ""
      end


      # strip envelope
      partTotal = partTotal.slice(position + 4, partTotal.length)

      # look for actual header & body
      partHeader, partBody = "", ""
      position = partTotal.index("\r\n\r\n");

      break if position == nil
      
      partHeader = partTotal.slice(0, position)
      partBody = partTotal.slice(position + 4, partTotal.length)

      partHeaders = { }
      status = 500
      lineNumber = 0

      # parse headers and status code

      partHeader.each_line("\r\n") do |line|
        if lineNumber == 0
          position = line.index("HTTP/1.1 ")
          break if position == nil
          status = line.slice(9, 3).to_i
        else
          key, void, value = line.partition(":")
          partHeaders[key.strip] = value.strip
        end
        lineNumber = lineNumber + 1
      end

      part = { :headers => partHeaders, :body => partBody, :status => status, :contentId => contentId }
      parts.push(part)

      body = body.slice(final, body.length)
    end

    if not valid
      raise "invalid multipart response received"
    end

    parts
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
      body += "Content-Type: application/x-arango-batchpart"

      if part[:contentId] != ""
        body += "\r\nContent-Id: " + part[:contentId]
      end
      
      body += "\r\n\r\n"

      # inner header
      body += part[:method] + " " + part[:url] + " HTTP/1.1\r\n"

      part[:headers].each do|key, value|
        body += key + ": " + value + "\r\n"
      end
      # header/body separator
      body += "\r\n"

      # body
      body += part[:body] + "\r\n"
    end 
    body += "--" + @boundary + "--\r\n"
  end

end
