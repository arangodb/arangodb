# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  prefix = "api-async"

  context "dealing with async requests:" do

################################################################################
## checking methods 
################################################################################
    
    it "checks whether async=false returns status 202" do
      cmd = "/_api/version"
      doc = ArangoDB.log_get("#{prefix}-get-status", cmd, :headers => { "X-Arango-Async" => "false" })

      doc.code.should eq(200)
      doc.headers.should_not have_key("x-arango-async-id")
      doc.response.body.should_not be_nil
    end

    it "checks whether async=true returns status 202" do
      cmd = "/_api/version"
      doc = ArangoDB.log_get("#{prefix}-get-status", cmd, :headers => { "X-Arango-Async" => "true" })

      doc.code.should eq(202)
      doc.headers.should_not have_key("x-arango-async-id")
      doc.response.body.should eq ""
    end
    
    it "checks whether async=1 returns status 202" do
      cmd = "/_api/version"
      doc = ArangoDB.log_get("#{prefix}-get-status", cmd, :headers => { "X-Arango-Async" => "1" })

      doc.code.should eq(200)
      doc.headers.should_not have_key("x-arango-async-id")
      doc.response.body.should_not be_nil
    end
    
    it "checks whether HEAD returns status 202" do
      cmd = "/_api/version"
      doc = ArangoDB.log_head("#{prefix}-head-status", cmd, :headers => { "X-Arango-Async" => "true" })

      doc.code.should eq(202)
      doc.headers.should_not have_key("x-arango-async-id")
      doc.response.body.should be_nil
    end
    
    it "checks whether POST returns status 202" do
      cmd = "/_api/version"
      doc = ArangoDB.log_post("#{prefix}-head-status", cmd, :body => "", :headers => { "X-Arango-Async" => "true" })

      doc.code.should eq(202)
      doc.headers.should_not have_key("x-arango-async-id")
      doc.response.body.should eq ""
    end
    
    it "checks whether an invalid location returns status 202" do
      cmd = "/_api/not-existing"
      doc = ArangoDB.log_get("#{prefix}-get-non-existing", cmd, :headers => { "X-Arango-Async" => "true" })

      doc.code.should eq(202)
      doc.headers.should_not have_key("x-arango-async-id")
      doc.response.body.should eq ""
    end
    
    it "checks whether a failing action returns status 202" do
      cmd = "/_admin/execute"
      body = "fail();"
      doc = ArangoDB.log_post("#{prefix}-post-failing", cmd, :body => body, :headers => { "X-Arango-Async" => "true" })

      doc.code.should eq(202)
      doc.headers.should_not have_key("x-arango-async-id")
      doc.response.body.should eq ""
    end
    
    it "checks the responses when the queue fills up" do
      cmd = "/_api/version"

      (1..500).each do
        doc = ArangoDB.log_get("#{prefix}-get-queue", cmd, :headers => { "X-Arango-Async" => "true" })

        doc.code.should eq(202)
        doc.headers.should_not have_key("x-arango-async-id")
        doc.response.body.should eq ""
      end
    end

    it "checks whether setting x-arango-async to 'store' returns a job id" do
      cmd = "/_api/version"
      doc = ArangoDB.log_get("#{prefix}-get-check-id", cmd, :headers => { "X-Arango-Async" => "store" })

      doc.code.should eq(202)
      doc.headers.should have_key("x-arango-async-id")
      doc.headers["x-arango-async-id"].should match(/^\d+$/)
      doc.response.body.should eq ""
    end

  end

end
