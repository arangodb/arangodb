# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do
  api = "/_api/document"
  prefix = "attributes"

  context "dealing with attribute names" do
      
    before do
      @cn = "UnitTestsCollectionAttributes"

      ArangoDB.drop_collection(@cn)
      @cid = ArangoDB.create_collection(@cn)
    end

    after do
      ArangoDB.drop_collection(@cn)
    end

################################################################################
## creates a document with an empty attribute 
################################################################################

    it "creates a document with an empty attribute name" do
      cmd = api + "?collection=" + @cn
      body = "{ \"\" : \"a\", \"foo\" : \"b\" }"
      doc = ArangoDB.log_post("#{prefix}-create-empty-name", cmd, :body => body)

      doc.code.should eq(201)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")

      doc.parsed_response['error'].should eq(false)

      id = doc.parsed_response['_id']

      cmd = api + "/" + id
      doc = ArangoDB.log_get("#{prefix}-create-empty-name", cmd)

      doc.parsed_response.should_not have_key('')
      doc.parsed_response.should have_key('foo')
    end

################################################################################
## queries a document with an empty attribute 
################################################################################

    it "queries a document with an empty attribute name" do
      cmd = api + "?collection=" + @cn
      body = "{ \"\" : \"a\", \"foo\" : \"b\" }"
      doc = ArangoDB.log_post("#{prefix}-query-empty-name", cmd, :body => body)

      doc.code.should eq(201)
      doc.parsed_response['error'].should eq(false)

      cmd = "/_api/simple/all"
      body = "{ \"collection\" : \"" + @cn + "\" }"
      doc = ArangoDB.log_put("#{prefix}-query-empty-name", cmd, :body => body)

      documents = doc.parsed_response['result']

      documents.length.should eq(1)
      documents[0].should_not have_key('')
      documents[0].should have_key('foo')
    end

################################################################################
## creates a document with reserved attribute names
################################################################################

    it "creates a document with reserved attribute names" do
      cmd = api + "?collection=" + @cn
      body = "{ \"_rev\" : \"99\", \"foo\" : \"002\", \"_id\" : \"meow\", \"_from\" : \"a\", \"_to\" : \"b\", \"_test\" : \"c\", \"meow\" : \"d\" }"
      doc = ArangoDB.log_post("#{prefix}-create-reserved-names", cmd, :body => body)

      doc.code.should eq(201)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")

      doc.parsed_response['error'].should eq(false)

      id = doc.parsed_response['_id']

      cmd = api + "/" + id
      doc = ArangoDB.log_get("#{prefix}-create-reserved-names", cmd)

      doc.parsed_response['_id'].should eq(id)
      doc.parsed_response['_rev'].should_not eq('99')
      doc.parsed_response.should_not have_key('_from')
      doc.parsed_response.should_not have_key('_to')
      doc.parsed_response.should_not have_key('_test')
      doc.parsed_response.should have_key('meow')
      doc.parsed_response['meow'].should eq('d')
      doc.parsed_response['foo'].should eq('002')
    end

################################################################################
## nested attribute names
################################################################################

    it "creates a document with nested attribute names" do
      cmd = api + "?collection=" + @cn
      body = "{ \"a\" : \"1\", \"b\" : { \"b\" : \"2\" , \"a\" : \"3\", \"\": \"4\", \"_from\": \"5\", \"c\" : 6 } }"
      doc = ArangoDB.log_post("#{prefix}-create-duplicate-names", cmd, :body => body)

      doc.code.should eq(201)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      doc.parsed_response['error'].should eq(false)
      
      id = doc.parsed_response['_id']

      cmd = api + "/" + id
      doc = ArangoDB.log_get("#{prefix}-create-empty-name", cmd)
      
      doc.parsed_response.should have_key('a')
      doc.parsed_response['a'].should eq('1')
      doc.parsed_response.should have_key('b')

      doc.parsed_response['b'].should_not have_key('')
      doc.parsed_response['b'].should_not have_key('_from')
      doc.parsed_response['b'].should have_key('b')
      doc.parsed_response['b'].should have_key('a')
      doc.parsed_response['b'].should have_key('c')
      doc.parsed_response['b'].should eq({ "b" => "2", "a" => "3", "c" => 6 })
    end

################################################################################
## duplicate attribute names
################################################################################

    it "creates a document with duplicate attribute names" do
      cmd = api + "?collection=" + @cn
      body = "{ \"a\" : \"1\", \"b\" : \"2\", \"a\" : \"3\" }"
      doc = ArangoDB.log_post("#{prefix}-create-duplicate-names", cmd, :body => body)

      doc.code.should eq(400)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")

      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['code'].should eq(400)
      doc.parsed_response['errorNum'].should eq(600)
    end

################################################################################
## nested duplicate attribute names
################################################################################

    it "creates a document with nested duplicate attribute names" do
      cmd = api + "?collection=" + @cn
      body = "{ \"a\" : \"1\", \"b\" : { \"b\" : \"2\" , \"c\" : \"3\", \"b\": \"4\" } }"
      doc = ArangoDB.log_post("#{prefix}-create-duplicate-names-nested", cmd, :body => body)

      doc.code.should eq(400)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")

      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['code'].should eq(400)
      doc.parsed_response['errorNum'].should eq(600)
    end

  end
end
