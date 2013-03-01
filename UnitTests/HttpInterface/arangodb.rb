# coding: utf-8

require 'rubygems'
require 'httparty'
require 'json'

$address = ENV['ARANGO_SERVER'] || '127.0.0.1:8529'

class ArangoDB
  include HTTParty

  if ENV['ARANGO_SSL'] == '1'
    base_uri "https://#{$address}"
  else
    base_uri "http://#{$address}"
  end 

  # set HTTP basic authorization
  basic_auth ENV['ARANGO_USER'], ENV['ARANGO_PASSWORD']

  # expect json as output/response format
  format :json

  # set timeout to 60 seconds
  default_timeout 60 

################################################################################
## create a collection
################################################################################

  def self.create_collection (name, wait_for_sync = true, type = 2)
    # type 2 means "document collection"
    body = "{ \"name\" : \"#{name}\", \"waitForSync\" : #{wait_for_sync}, \"type\" : #{type} }"

    doc = self.post("/_api/collection", :body => body)

    if doc.code == 409
      return doc.parsed_response['id']
    end

    if doc.code != 200
      return nil
    end

    return doc.parsed_response['id']
  end

################################################################################
## drop a collection
################################################################################

  def self.drop_collection (name)
    cmd = "/_api/collection/#{name}"
    self.delete(cmd)
  end

################################################################################
## size of a collection
################################################################################

  def self.size_collection (name)
    doc = self.get("/_api/collection/#{name}/count") # TODO use api call

    return doc.parsed_response['count']
  end

################################################################################
## issues a get request
################################################################################

  def self.log_get (output, url, args = {})
    doc = self.get(url, args);
    self.log(:method => :get, :url => url, :body => args[:body], :headers => args[:headers], :result => doc, :output => output, :format => args[:format]);
    return doc
  end

################################################################################
## issues a head request
################################################################################

  def self.log_head (output, url, args = {})
    doc = self.head(url, args);
    self.log(:method => :head, :url => url, :body => args[:body], :headers => args[:headers], :result => doc, :output => output, :format => args[:format]);
    return doc
  end

################################################################################
## issues an options request
################################################################################

  def self.log_options (output, url, args = {})
    doc = self.options(url, args);
    self.log(:method => :options, :url => url, :body => args[:body], :headers => args[:headers], :result => doc, :output => output, :format => args[:format]);
    return doc
  end

################################################################################
## issues a post request
################################################################################

  def self.log_post (output, url, args = {})
    doc = self.post(url, args);
    self.log(:method => :post, :url => url, :body => args[:body], :headers => args[:headers], :result => doc, :output => output, :format => args[:format]);
    return doc
  end

################################################################################
## issues a put request
################################################################################

  def self.log_put (output, url, args = {})
    doc = self.put(url, args);
    self.log(:method => :put, :url => url, :body => args[:body], :headers => args[:headers], :result => doc, :output => output, :format => args[:format]);
    return doc
  end

################################################################################
## issues a delete request
################################################################################

  def self.log_delete (output, url, args = {})
    doc = self.delete(url, args);
    self.log(:method => :delete, :url => url, :body => args[:body], :headers => args[:headers], :result => doc, :output => output, :format => args[:format]);
    return doc
  end

################################################################################
## issues a patch request
################################################################################

  def self.log_patch (output, url, args = {})
    doc = self.patch(url, args);
    self.log(:method => :patch, :url => url, :body => args[:body], :headers => args[:headers], :result => doc, :output => output, :format => args[:format]);
    return doc
  end

################################################################################
## generate log file
################################################################################

  def self.log (args)
    if args.key?(:output)
      logfile = File.new("logs/#{args[:output]}", "a")
    else
      logfile = File.new("output.log", "a")
    end

    method = args[:method] || :get
    url = args[:url]
    body = args[:body]
    headers = args[:headers]
    result = args[:result]
    response = result.parsed_response

    logfile.puts '-' * 80

    h_option = ""
    h_sep = ""

    if headers
      for k in [ "if-match", "if-none-match" ] do
        if headers.key?(k)
          h_option = h_option + h_sep + "'-H #{k}: #{headers[k]}'"
          h_sep = " "
        end
      end
      h_option = h_option + h_sep
    end

    if method == :get
      logfile.puts "> curl -X GET #{h_option}--dump - http://localhost:8529#{url}"
      logfile.puts
    elsif method == :head
      logfile.puts "> curl -X HEAD #{h_option}--dump - http://localhost:8529#{url}"
      logfile.puts
    elsif method == :delete
      logfile.puts "> curl -X DELETE #{h_option}--dump - http://localhost:8529#{url}"
      logfile.puts
    elsif method == :post
      if body == nil
        logfile.puts "> curl -X POST #{h_option}--dump - http://localhost:8529#{url}"
      else
        logfile.puts "> curl --data @- -X POST #{h_option}--dump - http://localhost:8529#{url}"
        logfile.puts body
      end
      logfile.puts
    elsif method == :put
      if body == nil
        logfile.puts "> curl -X PUT #{h_option}--dump - http://localhost:8529#{url}"
      else
        logfile.puts "> curl --data @- -X PUT #{h_option}--dump - http://localhost:8529#{url}"
        logfile.puts body
      end
      logfile.puts
    elsif method == :patch
      if body == nil
        logfile.puts "> curl -X PATCH #{h_option}--dump - http://localhost:8529#{url}"
      else
        logfile.puts "> curl --data @- -X PATCH #{h_option}--dump - http://localhost:8529#{url}"
        logfile.puts body
      end
      logfile.puts
    else
      logfile.puts "MISSING"
    end

    logfile.puts "HTTP/1.1 #{result.code} #{result.message}"

    if result.headers.key?('content-type')
      logfile.puts "content-type: #{result.headers['content-type']}"
    end

    if result.headers.key?('location')
      logfile.puts "location: #{result.headers['location']}"
    end

    if result.headers.key?('etag')
      logfile.puts "etag: #{result.headers['etag']}"
    end

    if response != nil
      format = "json"
      if args[:format] != nil
        format = args[:format]
      end

      if format == "json" 
        logfile.puts
        logfile.puts JSON.pretty_generate(response)
      end
    end

    logfile.close
  end
end
