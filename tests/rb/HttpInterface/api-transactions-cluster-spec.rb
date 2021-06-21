# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_api/transaction"
  prefix = "api-transaction"

  context "running server-side transactions:" do

################################################################################
## error handling
################################################################################

    context "error handling:" do
      it "returns an error if a wrong method type is used" do
        cmd = api
        body = "{ \"collections\" : { }, \"action\" : \" for \" }"
        doc = ArangoDB.log_patch("#{prefix}-invalid-method", cmd, :body => body)
        
        doc.code.should eq(405)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(405)
        doc.parsed_response['errorNum'].should eq(405)
      end

      it "returns an error if no body is posted" do
        cmd = api
        doc = ArangoDB.log_post("#{prefix}-missing-body", cmd)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(10)
      end

      it "returns an error if no collections attribute is present" do
        cmd = api
        body = "{ \"foo\" : \"bar\" }"
        doc = ArangoDB.log_post("#{prefix}-no-collection", cmd, :body => body)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(10)
      end

      it "returns an error if the collections attribute has a wrong type" do
        cmd = api
        doc = ArangoDB.log_post("#{prefix}-collection-wrong-type", cmd)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(10)
      end

      it "returns an error if collections sub-attribute is wrong" do
        cmd = api
        body = "{ \"collections\" : { \"write\": false } }"
        doc = ArangoDB.log_post("#{prefix}-invalid-collection-subattribute", cmd, :body => body)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(10)
      end

      it "returns an error if no action is specified" do
        cmd = api
        body = "{ \"collections\" : { } }"
        doc = ArangoDB.log_post("#{prefix}-missing-action", cmd, :body => body)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(10)
      end
      
      it "returns an error if action attribute has a wrong type" do
        cmd = api
        body = "{ \"collections\" : { }, \"action\" : false }"
        doc = ArangoDB.log_post("#{prefix}-invalid-action", cmd, :body => body)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(10)
      end
      
      it "returns an error if action attribute contains broken code 1" do
        cmd = api
        body = "{ \"collections\" : { }, \"action\" : \" for \" }"
        doc = ArangoDB.log_post("#{prefix}-invalid-action1", cmd, :body => body)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(10)
      end
      
      it "returns an error if action attribute contains broken code 2" do
        cmd = api
        body = "{ \"collections\" : { }, \"action\" : \"return 1;\" }"
        doc = ArangoDB.log_post("#{prefix}-invalid-action2", cmd, :body => body)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(10)
      end
      
      it "returns an error if action attribute contains broken code 3" do
        cmd = api
        body = "{ \"collections\" : { }, \"action\" : \"function() {\" }"
        doc = ArangoDB.log_post("#{prefix}-invalid-action3", cmd, :body => body)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(10)
      end

      it "returns an error if transactions are nested 1" do
        cmd = api
        body = "{ \"collections\" : { }, \"action\" : \"function () { return TRANSACTION({ collections: { }, action: function() { return 1; } }); }\" }"
        doc = ArangoDB.log_post("#{prefix}-nested1", cmd, :body => body)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1651)
      end

      it "returns an error if transactions are nested 2" do
        cmd = api
        body = "{ \"collections\" : { }, \"action\" : \"function () { return TRANSACTION({ collections: { }, action: \\\"function () { return 1; }\\\" }); }\" }"
        doc = ArangoDB.log_post("#{prefix}-nested2", cmd, :body => body)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1651)
      end

    end

################################################################################
## using "wrong" collections
################################################################################

    context "using wrong collections:" do
      before do
        @cn1 = "UnitTestsTransactions1"
        @cn2 = "UnitTestsTransactions2"
        ArangoDB.create_collection(@cn1, false)
        ArangoDB.create_collection(@cn2, false)
      end

      after do
        ArangoDB.drop_collection(@cn1)
        ArangoDB.drop_collection(@cn2)
      end

      it "returns an error if referring to a non-existing collection" do
        cmd = api
        body = "{ \"collections\" : { \"write\": \"_meow\" }, \"action\" : \"function () { return 1; }\" }"
        doc = ArangoDB.log_post("#{prefix}-non-existing-collection", cmd, :body => body)
        
        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
        doc.parsed_response['errorNum'].should eq(1203)
      end
      
      it "returns an error when using a disallowed operation" do
        cmd = api
        body = "{ \"collections\" : { }, \"action\" : \"function () { require(\\\"internal\\\").db._create(\\\"abc\\\"); }\" }"
        doc = ArangoDB.log_post("#{prefix}-invalid-mode2", cmd, :body => body)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1653)
      end
    end

################################################################################
## params
################################################################################
    
    context "using parameters:" do
      it "checking return parameters" do
        cmd = api
        body = "{ \"collections\" : { }, \"action\" : \"function (params) { return [ params[1], params[4] ]; }\", \"params\" : [ 1, 2, 3, 4, 5 ] }"
        doc = ArangoDB.log_post("#{prefix}-params1", cmd, :body => body)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['result'].should eq([ 2, 5 ])
      end

      it "checking return parameters, other argument name" do
        cmd = api
        body = "{ \"collections\" : { }, \"action\" : \"function (args) { return [ args[1], args[4] ]; }\", \"params\" : [ 1, 2, 3, 4, 5 ] }"
        doc = ArangoDB.log_post("#{prefix}-params2", cmd, :body => body)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['result'].should eq([ 2, 5 ])
      end
      
      it "checking return parameters, object 1" do
        cmd = api
        body = "{ \"collections\" : { }, \"action\" : \"function (params) { return params['foo']; }\", \"params\" : { \"foo\" : \"bar\" } }"
        doc = ArangoDB.log_post("#{prefix}-params3", cmd, :body => body)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['result'].should eq("bar")
      end
      
      it "checking return parameters, object 2" do
        cmd = api
        body = "{ \"collections\" : { }, \"action\" : \"function (params) { return params['meow']; }\", \"params\" : { \"foo\" : \"bar\" } }"
        doc = ArangoDB.log_post("#{prefix}-params4", cmd, :body => body)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['result'].should eq(nil)
      end

      it "checking return parameters, undefined 1" do
        cmd = api
        body = "{ \"collections\" : { }, \"action\" : \"function (params) { return params['meow']; }\", \"params\" : null }"
        doc = ArangoDB.log_post("#{prefix}-params5", cmd, :body => body)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(17) # TypeError
      end
      
      it "checking return parameters, undefined 2" do
        cmd = api
        body = "{ \"collections\" : { }, \"action\" : \"function (params) { return params['meow']; }\", \"params\" : { } }"
        doc = ArangoDB.log_post("#{prefix}-params5", cmd, :body => body)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['result'].should eq(nil)
      end
      
      it "checking return parameters, undefined 3" do
        cmd = api
        body = "{ \"collections\" : { }, \"action\" : \"function (params) { return params; }\" }"
        doc = ArangoDB.log_post("#{prefix}-params7", cmd, :body => body)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['result'].should eq(nil)
      end
    end

################################################################################
## non-collection transactions
################################################################################

    context "non-collection transactions:" do
      it "returning a simple type" do
        cmd = api
        body = "{ \"collections\" : { }, \"action\" : \"function () { return 42; }\" }"
        doc = ArangoDB.log_post("#{prefix}-empty-transaction1", cmd, :body => body)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['result'].should eq(42)
      end

      it "returning a compound type" do
        cmd = api
        body = "{ \"collections\" : { }, \"action\" : \"function () { return [ true, { a: 42, b: [ null, true ], c: \\\"foo\\\" } ]; }\" }"
        doc = ArangoDB.log_post("#{prefix}-empty-transaction2", cmd, :body => body)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['result'].should eq([ true, { "a" => 42, "b" => [ nil, true ], "c" => "foo" } ])
      end

      it "returning an exception" do
        cmd = api
        body = "{ \"collections\" : { }, \"action\" : \"function () { throw \\\"doh!\\\"; }\" }"
        doc = ArangoDB.log_post("#{prefix}-empty-exception", cmd, :body => body)
        
        doc.code.should eq(500)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(500)
        doc.parsed_response['errorNum'].should eq(1650)
      end
    end

################################################################################
## single-collection transactions
################################################################################

    context "single collection transactions:" do
      before do
        @cn = "UnitTestsTransactions"
        @cid = ArangoDB.create_collection(@cn, false)
      end
      
      after do
        ArangoDB.drop_collection(@cn)
      end

      it "read-only, using write 1" do
        body = "{ }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        
        ArangoDB.size_collection(@cn).should eq(3);

        cmd = api
        body = "{ \"collections\" : { \"write\": \"#{@cn}\" }, \"action\" : \"function () { var c = require(\\\"internal\\\").db.UnitTestsTransactions; return c.count(); }\" }"
        doc = ArangoDB.log_post("#{prefix}-read-write", cmd, :body => body)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['result'].should eq(3)

        ArangoDB.size_collection(@cn).should eq(3);
      end

      it "read-only, using read 1" do
        body = "{ }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        
        ArangoDB.size_collection(@cn).should eq(3);

        cmd = api
        body = "{ \"collections\" : { \"read\": \"#{@cn}\" }, \"action\" : \"function () { var c = require(\\\"internal\\\").db.UnitTestsTransactions; return c.count(); }\" }"
        doc = ArangoDB.log_post("#{prefix}-read-only", cmd, :body => body)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['result'].should eq(3)

        ArangoDB.size_collection(@cn).should eq(3);
      end

      it "committing 1" do
        cmd = api
        body = "{ \"collections\" : { \"write\": \"#{@cn}\" }, \"action\" : \"function () { var c = require(\\\"internal\\\").db.UnitTestsTransactions; c.save({ }); c.save({ }); return c.count(); }\" }"
        doc = ArangoDB.log_post("#{prefix}-single-commit", cmd, :body => body)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['result'].should eq(2)

        ArangoDB.size_collection(@cn).should eq(2);
      end

    end

################################################################################
## multi-collection transactions
################################################################################

    context "multi collection transactions:" do
      before do
        @cn1 = "UnitTestsTransactions1"
        @cn2 = "UnitTestsTransactions2"
        ArangoDB.create_collection(@cn1, false)
        ArangoDB.create_collection(@cn2, false)
      end
      
      after do
        ArangoDB.drop_collection(@cn1)
        ArangoDB.drop_collection(@cn2)
      end

      it "read-only, using write 2" do
        body = "{ }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn1}", :body => body)
        doc = ArangoDB.post("/_api/document?collection=#{@cn1}", :body => body)
        doc = ArangoDB.post("/_api/document?collection=#{@cn1}", :body => body)
        doc = ArangoDB.post("/_api/document?collection=#{@cn2}", :body => body)
        
        ArangoDB.size_collection(@cn1).should eq(3);
        ArangoDB.size_collection(@cn2).should eq(1);

        cmd = api
        body = "{ \"collections\" : { \"write\": [ \"#{@cn1}\", \"#{@cn2}\" ] }, \"action\" : \"function () { var c1 = require(\\\"internal\\\").db.#{@cn1}; var c2 = require(\\\"internal\\\").db.#{@cn2}; return [ c1.count(), c2.count() ]; }\" }"
        doc = ArangoDB.log_post("#{prefix}-multi-read-write", cmd, :body => body)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['result'].should eq([ 3, 1 ])

        ArangoDB.size_collection(@cn1).should eq(3);
        ArangoDB.size_collection(@cn2).should eq(1);
      end

      it "read-only, using read 2" do
        body = "{ }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn1}", :body => body)
        doc = ArangoDB.post("/_api/document?collection=#{@cn1}", :body => body)
        doc = ArangoDB.post("/_api/document?collection=#{@cn1}", :body => body)
        doc = ArangoDB.post("/_api/document?collection=#{@cn2}", :body => body)
        
        ArangoDB.size_collection(@cn1).should eq(3);
        ArangoDB.size_collection(@cn2).should eq(1);

        cmd = api
        body = "{ \"collections\" : { \"read\": [ \"#{@cn1}\", \"#{@cn2}\" ] }, \"action\" : \"function () { var c1 = require(\\\"internal\\\").db.#{@cn1}; var c2 = require(\\\"internal\\\").db.#{@cn2}; return [ c1.count(), c2.count() ]; }\" }"
        doc = ArangoDB.log_post("#{prefix}-multi-read-only", cmd, :body => body)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['result'].should eq([ 3, 1 ])

        ArangoDB.size_collection(@cn1).should eq(3);
        ArangoDB.size_collection(@cn2).should eq(1);
      end

      it "committing 2" do
        cmd = api
        body = "{ \"collections\" : { \"write\": [ \"#{@cn1}\", \"#{@cn2}\" ] }, \"action\" : \"function () { var c1 = require(\\\"internal\\\").db.#{@cn1}; var c2 = require(\\\"internal\\\").db.#{@cn2}; c1.save({ }); c1.save({ }); c2.save({ }); return [ c1.count(), c2.count() ]; }\" }"
        doc = ArangoDB.log_post("#{prefix}-multi-commit", cmd, :body => body)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")

        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['result'].should eq([ 2, 1 ])

        ArangoDB.size_collection(@cn1).should eq(2);
        ArangoDB.size_collection(@cn2).should eq(1);
      end

    end

  end
end
