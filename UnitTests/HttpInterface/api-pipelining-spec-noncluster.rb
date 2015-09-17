# coding: utf-8

require 'rspec'
require 'socket'
require 'arangodb.rb'

def read_socket (socket) 
  response = ""
        
  while true
    rs = IO.select([socket], [ ], [ ], 1.0)

    if rs === nil 
      break
    end

    partial = socket.recv(8192)

    if partial.length == 0
      break
    end

    response << partial
  end

  response
end

def await_response (n)
  response = ""
  while true
    part = read_socket @socket
    if part != ""
      response += part
      if response.scan(/HTTP\/1\.1 20[012]/).length === n
        break
      end
    end
  end
  response
end

describe ArangoDB, :ssl => true do

  context "dealing with HTTP pipelining:" do
      
    before do
      parts = $address.split(':', 2)

      address = parts[0]
      port = parts[1] || 8529

      @socket = TCPSocket.open(address, port)
    end

    after do
      @socket.close
    end


################################################################################
## checking direct handlers
################################################################################

    context "using direct handlers:" do

      it "simple requests, no content-length" do
        requests = ""
        
        n = 10

        (0...n).each do |i|
          requests << "GET /_api/version HTTP/1.1\r\n\r\n"
        end

        @socket.send requests, 0

        response = await_response n
        response.scan(/HTTP\/1\.1 200/).length.should eq(n)
      end
      
      it "simple requests, with content-length" do
        requests = ""
        
        n = 10

        (0...n).each do |i|
          requests << "GET /_api/version HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n"
        end

        @socket.send requests, 0

        response = await_response n
        response.scan(/HTTP\/1\.1 200/).length.should eq(n)
      end
      
      it "many requests" do
        requests = ""
        
        n = 1000

        (0...n).each do |i|
          requests << "GET /_api/version HTTP/1.1\r\n\r\n"
        end

        @socket.send requests, 0

        response = await_response n
        response.scan(/HTTP\/1\.1 200/).length.should eq(n)
      end
      
    end

################################################################################
## checking indirect handlers
################################################################################

    context "using indirect handlers:" do
      
      before do
        @cn = "UnitTestsCollection"

        ArangoDB.drop_collection(@cn)
        ArangoDB.create_collection(@cn)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "checks post requests" do
        n = 10

        requests = ""
        (0...n).each do |i|
          body = "{ \"value\" : #{i} }"
          requests << "POST /_api/document?collection=#{@cn} HTTP/1.1\r\nContent-Length: "
          requests << body.length.to_s
          requests << "\r\n\r\n"
          requests << body
        end

        @socket.send requests, 0

        response = await_response n
        response.scan(/HTTP\/1\.1 201/).length.should eq(n)
      end
      
      it "checks post and get requests" do
        n = 500

        requests = ""
        (0...n).each do |i|
          body = "{ \"_key\" : \"test#{i}\", \"value\" : #{i} }"
          requests << "POST /_api/document?collection=#{@cn} HTTP/1.1\r\nContent-Length: "
          requests << body.length.to_s
          requests << "\r\n\r\n"
          requests << body
        end

        @socket.send requests, 0
        
        requests = ""
        (0...n).each do |i|
          requests << "GET /_api/document/#{@cn}/test#{i} HTTP/1.1\r\n\r\n"
        end

        @socket.send requests, 0

        response = await_response n * 2
        response.scan(/HTTP\/1\.1 20[012]/).length.should eq(n * 2)
      end
      
    end

  end

end
