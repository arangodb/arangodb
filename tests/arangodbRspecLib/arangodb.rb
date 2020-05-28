# coding: utf-8

require 'rubygems'
require 'persistent_httparty'
require 'json'
require 'rspec'
require 'rspec/expectations'


$address = ENV['ARANGO_SERVER'] || '127.0.0.1:8529'
$user = ENV['ARANGO_USER']
$password = ENV['ARANGO_PASSWORD']
$ssl = ENV['ARANGO_SSL']
$silent = ENV['ARANGO_NO_LOG'] || ''
$skip_timecritical = ENV['SKIP_TIMECRITICAL'] || true

begin
  $address = RSpec.configuration.ARANGO_SERVER
rescue
end
begin
  $user = RSpec.configuration.ARANGO_USER
rescue
end
begin
  $password = RSpec.configuration.ARANGO_PASSWORD
rescue
end
begin
  $ssl = RSpec.configuration.ARANGO_SSL
rescue
end
begin
  $skip_timecritical = RSpec.configuration.SKIP_TIMECRITICAL
rescue
end

RSpec.configure do |config|
  config.expect_with :rspec do |c|
    c.syntax = [ :should, :expect ]
  end
  config.mock_with :rspec do |c|
    c.syntax = [ :should, :expect ]
  end
  # tests to be excluded when under ssl
  if $ssl == '1'
    config.filter_run_excluding :ssl => true
  end
  if $skip_timecritical
    config.filter_run_excluding :timecritical => true
  end
end

################################################################################
## cross rspec version compatibility tool: 
################################################################################
RSpec::Matchers.define :be_nil_or_empty do
 match do |actual|
   actual.nil? || actual.empty?
 end
end

class ArangoDB
  include HTTParty
  persistent_connection_adapter

  if $ssl == '1'
    base_uri "https://#{$address}"
  else
    base_uri "http://#{$address}"
  end 

  # set HTTP basic authorization
  basic_auth $user, $password

  # expect json as output/response format
  format :json

  # set timeout to 300 seconds - so we will see timeouts in the cluster with ther
  # respective error messages from the coordinator.
  default_timeout 300 

  # do not verify SSL
  default_options[:verify] = false

################################################################################
## adjust the global client-side timeout value
################################################################################

  def self.set_timeout (value) 
    old_value = default_options[:timeout] 
    default_options[:timeout] = value
    old_value
  end

  def self.set_read_timeout (value) 
    old_value = default_options[:read_timeout] 
    default_options[:read_timeout] = value
    old_value
  end
  
################################################################################
## create a database
################################################################################

  def self.create_database (name)
    body = "{ \"name\" : \"#{name}\" }"

    doc = self.post("/_api/database", :body => body)

    if doc.code != 201
      return false
    end

    return true
  end

################################################################################
## drop a database
################################################################################

  def self.drop_database (name)
    body = ""

    doc = self.delete("/_api/database/#{name}", :body => body)

    if doc.code != 200
      return false
    end

    return true
  end

################################################################################
## create a collection
################################################################################

  def self.create_collection (name, wait_for_sync = true, type = 2, dbname = "_system")
    # type 2 means "document collection"
    body = "{ \"name\" : \"#{name}\", \"waitForSync\" : #{wait_for_sync}, \"type\" : #{type} }"

    doc = self.post("/_db/#{dbname}/_api/collection", :body => body)

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

  def self.drop_collection (name, dbname = "_system")
    cmd = "/_db/#{dbname}/_api/collection/#{name}"
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
## properties of a collection
################################################################################

def self.properties_collection (name)
  doc = self.get("/_api/collection/#{name}/properties") # TODO use api call

  return doc.parsed_response
end

################################################################################
## create a single collection graph
################################################################################

  def self.create_single_collection_graph (name, edgeCollection, vertexCollection)
  edge_definitions = {:collection => edgeCollection, :from => [vertexCollection], :to => [vertexCollection]}
  body = JSON.dump({:name => name, :edgeDefinitions => [edge_definitions]})

    doc = self.post("/_api/gharial", :body => body)

    return doc
  end

################################################################################
## drop a graph
################################################################################

  def self.drop_graph (name)
    cmd = "/_api/gharial/#{name}?dropCollections=true"
    self.delete(cmd)
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
    # disable logging if requested
    result = args[:result]
    response = result.parsed_response
    return if not ($silent.nil? || $silent.empty? || $silent == "0")

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
      for k in headers.keys do
        h_option = h_option + h_sep + "'-H #{k}: #{headers[k]}'"
        h_sep = " "
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

    if result.headers.key?('x-arango-async-id')
      logfile.puts "x-arango-async-id: #{result.headers['x-arango-async-id']}"
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

# super carrot fix for strange SSL behavior on windows
# on some windowses the first SSL request somehow throws strange SSL errors
# after that everything is fine...so make a dummy request first
if $ssl == '1'
  retries = 0
  begin
    ArangoDB.get("/_api/version")
  rescue
    retries+=1
    if retries < 5 then
      retry
    else
      raise
    end
  end
end
