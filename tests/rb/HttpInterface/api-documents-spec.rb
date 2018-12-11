# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_api/document"
  prefix = "documents"

  context "dealing with documents" do
      
    before do
      @cn = "UnitTestsCollectionDocuments"

      ArangoDB.drop_collection(@cn)
      @cid = ArangoDB.create_collection(@cn)
    end

    after do
      ArangoDB.drop_collection(@cn)
    end

################################################################################
## creates documents with invalid types
################################################################################

    it "creates a document with an invalid type" do
      cmd = api + "?collection=" + @cn
      body = "[ [] ]";
      doc = ArangoDB.log_post("#{prefix}-create-list1", cmd, :body => body)

      doc.code.should eq(202)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      doc.parsed_response[0]["error"].should eq(true)
      doc.parsed_response[0]["errorNum"].should eq(1227)
    end
    
    it "creates a document with an invalid type" do
      cmd = api + "?collection=" + @cn
      body = "\"test\"";
      doc = ArangoDB.log_post("#{prefix}-create-list2", cmd, :body => body)

      doc.code.should eq(400)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")

      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['code'].should eq(400)
      doc.parsed_response['errorNum'].should eq(1227)
    end

################################################################################
## updates documents with invalid types
################################################################################

    it "updates a document with an invalid type" do
      cmd = api + "/#{@cn}/test"
      body = "[ ]";
      doc = ArangoDB.log_patch("#{prefix}-update-object1", cmd, :body => body)

      doc.code.should eq(400)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")

      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['code'].should eq(400)
      doc.parsed_response['errorNum'].should eq(1227)
    end
    
    it "updates a document with an invalid type" do
      cmd = api + "/#{@cn}/test"
      body = "\"test\"";
      doc = ArangoDB.log_patch("#{prefix}-update-object2", cmd, :body => body)

      doc.code.should eq(400)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")

      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['code'].should eq(400)
      doc.parsed_response['errorNum'].should eq(1227)
    end

################################################################################
## replaces documents with invalid types
################################################################################

    it "replaces a document with an invalid type" do
      cmd = api + "/#{@cn}/test"
      body = "[ ]";
      doc = ArangoDB.log_put("#{prefix}-replace-object1", cmd, :body => body)

      doc.code.should eq(400)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")

      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['code'].should eq(400)
      doc.parsed_response['errorNum'].should eq(1227)
    end
    
    it "replaces a document with an invalid type" do
      cmd = api + "/#{@cn}/test"
      body = "\"test\"";
      doc = ArangoDB.log_put("#{prefix}-replace-object2", cmd, :body => body)

      doc.code.should eq(400)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")

      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['code'].should eq(400)
      doc.parsed_response['errorNum'].should eq(1227)
    end

################################################################################
## updates documents by example with invalid type
################################################################################

    it "updates documents by example with an invalid type" do
      cmd = "/_api/simple/update-by-example"
      body = "{ \"collection\" : \"#{@cn}\", \"example\" : [ ], \"newValue\" : { } }";
      doc = ArangoDB.log_put("#{prefix}-update-by-example1", cmd, :body => body)

      doc.code.should eq(400)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")

      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['code'].should eq(400)
      doc.parsed_response['errorNum'].should eq(400)
    end
    
    it "updates documents by example with an invalid type" do
      cmd = "/_api/simple/update-by-example"
      body = "{ \"collection\" : \"#{@cn}\", \"example\" : { }, \"newValue\" : [ ] }";
      doc = ArangoDB.log_put("#{prefix}-update-by-example2", cmd, :body => body)

      doc.code.should eq(400)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")

      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['code'].should eq(400)
      doc.parsed_response['errorNum'].should eq(400)
    end

################################################################################
## replaces documents by example with invalid type
################################################################################

    it "replaces documents by example with an invalid type" do
      cmd = "/_api/simple/replace-by-example"
      body = "{ \"collection\" : \"#{@cn}\", \"example\" : [ ], \"newValue\" : { } }";
      doc = ArangoDB.log_put("#{prefix}-replace-by-example1", cmd, :body => body)

      doc.code.should eq(400)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")

      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['code'].should eq(400)
      doc.parsed_response['errorNum'].should eq(400)
    end
    
    it "replaces documents by example with an invalid type" do
      cmd = "/_api/simple/replace-by-example"
      body = "{ \"collection\" : \"#{@cn}\", \"example\" : { }, \"newValue\" : [ ] }";
      doc = ArangoDB.log_put("#{prefix}-replace-by-example2", cmd, :body => body)

      doc.code.should eq(400)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")

      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['code'].should eq(400)
      doc.parsed_response['errorNum'].should eq(400)
    end

################################################################################
## removes documents by example with invalid type
################################################################################

    it "removes a document with an invalid type" do
      cmd = "/_api/simple/remove-by-example"
      body = "{ \"collection\" : \"#{@cn}\", \"example\" : [ ] }";
      doc = ArangoDB.log_put("#{prefix}-remove-by-example", cmd, :body => body)

      doc.code.should eq(400)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")

      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['code'].should eq(400)
      doc.parsed_response['errorNum'].should eq(400)
    end
    
  end
end
