# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_admin/"
  prefix = "admin-statistics"
  context "calculating statistics:" do

################################################################################
## check statistics-description availability
###############################################################################
  
    it "testing statistics-description correct cmd" do 
      cmd = "/_admin/statistics-description"
      doc = ArangoDB.log_get("#{prefix}", cmd) 
  
      doc.code.should eq(200)
    end

################################################################################
## check statistics-description for wrong user interaction
###############################################################################

    it "testing statistics-description wrong cmd" do 
      cmd = "/_admin/statistics-description/asd123"
      doc = ArangoDB.log_get("#{prefix}", cmd) 
   
      doc.code.should eq(404)
      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['errorNum'].should eq(404)
    end

################################################################################
## check statistics availability
###############################################################################

    it "testing statistics correct cmd" do 
      cmd = "/_admin/statistics"
      doc = ArangoDB.log_get("#{prefix}", cmd) 
      doc.code.should eq(200)
      doc.parsed_response['server']['uptime'].should be > 0
    end

################################################################################
## check statistics for wrong user interaction
###############################################################################

    it "testing statistics wrong cmd" do 
      cmd = "/_admin/statistics/asd123"
      doc = ArangoDB.log_get("#{prefix}", cmd) 
   
      doc.code.should eq(404)
      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['errorNum'].should eq(404)
    end

################################################################################
## check request statistics counting of async requests
###############################################################################

    it "testing async requests " do
      # get stats
      cmd = "/_admin/statistics"
      doc = ArangoDB.log_get("#{prefix}-get-statistics", cmd)
      doc.code.should eq(200)
      async_requests_1 = doc.parsed_response['http']['requestsAsync'].to_i

      # get version async - should increase async counter
      cmd = "/_api/version"
      doc = ArangoDB.log_put("#{prefix}-put-async-request", cmd, :headers => { "X-Arango-Async" => "true" })
      doc.code.should eq(202)
      #doc.headers.should have_key("x-arango-async-id")
      #doc.headers["x-arango-async-id"].should match(/^\d+$/)
      doc.response.body.should eq ""

      sleep 1

      # get stats
      cmd = "/_admin/statistics"
      doc = ArangoDB.log_get("#{prefix}-get-statistics", cmd)
      doc.code.should eq(200)
      async_requests_2 = doc.parsed_response['http']['requestsAsync'].to_i
      async_requests_1.should be < async_requests_2

      # get version async - should not increase async counter
      cmd = "/_api/version"
      doc = ArangoDB.log_put("#{prefix}-put-async-request", cmd)
      doc.code.should eq(200)

      sleep 1

      # get stats
      cmd = "/_admin/statistics"
      doc = ArangoDB.log_get("#{prefix}-get-statistics", cmd)
      doc.code.should eq(200)
      async_requests_3 = doc.parsed_response['http']['requestsAsync'].to_i
      async_requests_2.should eq async_requests_3
    end

  end
end

