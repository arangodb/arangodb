# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_api/index"
  prefix = "api-index"

  context "dealing with indexes:" do
    before do
      @reFull = Regexp.new('^[a-zA-Z0-9_\-]+/\d+$')
    end

################################################################################
## error handling
################################################################################

    context "error handling:" do
      before do
        @cn = "UnitTestsCollectionIndexes"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "returns an error if collection identifier is unknown" do
        cmd = api + "/123456/123456"
        doc = ArangoDB.log_get("#{prefix}-bad-collection-identifier", cmd)

        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1203)
        doc.parsed_response['code'].should eq(404)
      end

      it "returns an error if index identifier is unknown" do
        cmd = api + "/#{@cn}/123456"
        doc = ArangoDB.log_get("#{prefix}-bad-index-identifier", cmd)

        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1212)
        doc.parsed_response['code'].should eq(404)
      end
    end

################################################################################
## creating a unique constraint
################################################################################

    context "creating unique constraints:" do
      before do
        @cn = "UnitTestsCollectionIndexes"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "returns either 201 for new or 200 for old indexes" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"hash\", \"unique\" : true, \"fields\" : [ \"a\", \"b\" ] }"
        doc = ArangoDB.log_post("#{prefix}-create-new-unique-constraint", cmd, :body => body)
  
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['type'].should eq("hash")
        doc.parsed_response['unique'].should eq(true)
        doc.parsed_response['sparse'].should eq(false)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
        doc.parsed_response['isNewlyCreated'].should eq(true)
        
        iid = doc.parsed_response['id']

        doc = ArangoDB.log_post("#{prefix}-create-old-unique-constraint", cmd, :body => body)
  
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['id'].should eq(iid)
        doc.parsed_response['type'].should eq("hash")
        doc.parsed_response['unique'].should eq(true)
        doc.parsed_response['sparse'].should eq(false)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
        doc.parsed_response['isNewlyCreated'].should eq(false)
      end

      it "returns either 201 for new or 200 for old indexes, sparse indexes" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"hash\", \"unique\" : true, \"fields\" : [ \"a\", \"b\" ], \"sparse\" : true }"
        doc = ArangoDB.log_post("#{prefix}-create-new-unique-constraint", cmd, :body => body)
  
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['type'].should eq("hash")
        doc.parsed_response['unique'].should eq(true)
        doc.parsed_response['sparse'].should eq(true)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
        doc.parsed_response['isNewlyCreated'].should eq(true)
        
        iid = doc.parsed_response['id']

        doc = ArangoDB.log_post("#{prefix}-create-old-unique-constraint", cmd, :body => body)
  
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['id'].should eq(iid)
        doc.parsed_response['type'].should eq("hash")
        doc.parsed_response['unique'].should eq(true)
        doc.parsed_response['sparse'].should eq(true)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
        doc.parsed_response['isNewlyCreated'].should eq(false)
      end
    end

################################################################################
## creating an unique constraint and unloading
################################################################################

    context "unique constraints after unload/load:" do
      before do
        @cn = "UnitTestsCollectionIndexes"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "survives unload" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"hash\", \"unique\" : true, \"fields\" : [ \"a\", \"b\" ] }"
        doc = ArangoDB.post(cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)
      end

      it "survives unload, sparse index" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"hash\", \"unique\" : true, \"fields\" : [ \"a\", \"b\" ], \"sparse\" : true }"
        doc = ArangoDB.post(cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)

        iid = doc.parsed_response['id']

        cmd = api + "/#{iid}"
        doc = ArangoDB.get(cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['id'].should eq(iid)
        doc.parsed_response['type'].should eq("hash")
        doc.parsed_response['unique'].should eq(true)
        doc.parsed_response['sparse'].should eq(true)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
      end
    end

################################################################################
## creating a hash index
################################################################################

    context "creating hash indexes:" do
      before do
        @cn = "UnitTestsCollectionIndexes"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "returns either 201 for new or 200 for old indexes" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"hash\", \"unique\" : false, \"fields\" : [ \"a\", \"b\" ] }"
        doc = ArangoDB.log_post("#{prefix}-create-new-hash-index", cmd, :body => body)
  
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['type'].should eq("hash")
        doc.parsed_response['unique'].should eq(false)
        doc.parsed_response['sparse'].should eq(false)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
        doc.parsed_response['isNewlyCreated'].should eq(true)

        iid = doc.parsed_response['id']

        doc = ArangoDB.log_post("#{prefix}-create-old-hash-index", cmd, :body => body)
  
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['id'].should eq(iid)
        doc.parsed_response['type'].should eq("hash")
        doc.parsed_response['unique'].should eq(false)
        doc.parsed_response['sparse'].should eq(false)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
        doc.parsed_response['isNewlyCreated'].should eq(false)
      end

      it "returns either 201 for new or 200 for old indexes, sparse index" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"hash\", \"unique\" : false, \"fields\" : [ \"a\", \"b\" ], \"sparse\" : true }"
        doc = ArangoDB.log_post("#{prefix}-create-new-hash-index", cmd, :body => body)
  
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['type'].should eq("hash")
        doc.parsed_response['unique'].should eq(false)
        doc.parsed_response['sparse'].should eq(true)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
        doc.parsed_response['isNewlyCreated'].should eq(true)

        iid = doc.parsed_response['id']

        doc = ArangoDB.log_post("#{prefix}-create-old-hash-index", cmd, :body => body)
  
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['id'].should eq(iid)
        doc.parsed_response['type'].should eq("hash")
        doc.parsed_response['unique'].should eq(false)
        doc.parsed_response['sparse'].should eq(true)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
        doc.parsed_response['isNewlyCreated'].should eq(false)
      end

      it "returns either 201 for new or 200 for old indexes, mixed sparsity" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"hash\", \"unique\" : false, \"fields\" : [ \"a\", \"b\" ] }"
        doc = ArangoDB.log_post("#{prefix}-create-new-hash-index", cmd, :body => body)
  
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['type'].should eq("hash")
        doc.parsed_response['unique'].should eq(false)
        doc.parsed_response['sparse'].should eq(false)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
        doc.parsed_response['isNewlyCreated'].should eq(true)

        iid = doc.parsed_response['id']

        body = "{ \"type\" : \"hash\", \"unique\" : false, \"fields\" : [ \"a\", \"b\" ], \"sparse\" : true }"
        doc = ArangoDB.log_post("#{prefix}-create-old-hash-index", cmd, :body => body)
  
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['id'].should_not eq(iid)
        doc.parsed_response['type'].should eq("hash")
        doc.parsed_response['unique'].should eq(false)
        doc.parsed_response['sparse'].should eq(true)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
        doc.parsed_response['isNewlyCreated'].should eq(true)
      end
    end

################################################################################
## creating a hash index and unloading
################################################################################

    context "hash indexes after unload/load:" do
      before do
        @cn = "UnitTestsCollectionIndexes"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "survives unload" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"hash\", \"unique\" : false, \"fields\" : [ \"a\", \"b\" ] }"
        doc = ArangoDB.post(cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)

        iid = doc.parsed_response['id']

        cmd = api + "/#{iid}"
        doc = ArangoDB.get(cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['id'].should eq(iid)
        doc.parsed_response['type'].should eq("hash")
        doc.parsed_response['unique'].should eq(false)
        doc.parsed_response['sparse'].should eq(false)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
      end

      it "survives unload, sparse index" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"hash\", \"unique\" : false, \"fields\" : [ \"a\", \"b\" ], \"sparse\" : true }"
        doc = ArangoDB.post(cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)

        iid = doc.parsed_response['id']

        cmd = api + "/#{iid}"
        doc = ArangoDB.get(cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['id'].should eq(iid)
        doc.parsed_response['type'].should eq("hash")
        doc.parsed_response['unique'].should eq(false)
        doc.parsed_response['sparse'].should eq(true)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
      end
    end

################################################################################
## creating a skiplist
################################################################################

    context "creating skiplists:" do
      before do
        @cn = "UnitTestsCollectionIndexes"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "returns either 201 for new or 200 for old indexes" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"skiplist\", \"unique\" : false, \"fields\" : [ \"a\", \"b\" ] }"
        doc = ArangoDB.log_post("#{prefix}-create-new-skiplist", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['type'].should eq("skiplist")
        doc.parsed_response['unique'].should eq(false)
        doc.parsed_response['sparse'].should eq(false)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
        doc.parsed_response['isNewlyCreated'].should eq(true)

        iid = doc.parsed_response['id']

        doc = ArangoDB.log_post("#{prefix}-create-old-skiplist", cmd, :body => body)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['id'].should eq(iid)
        doc.parsed_response['type'].should eq("skiplist")
        doc.parsed_response['unique'].should eq(false)
        doc.parsed_response['sparse'].should eq(false)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
        doc.parsed_response['isNewlyCreated'].should eq(false)
      end

      it "returns either 201 for new or 200 for old indexes, sparse index" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"skiplist\", \"unique\" : false, \"fields\" : [ \"a\", \"b\" ], \"sparse\" : true }"
        doc = ArangoDB.log_post("#{prefix}-create-new-skiplist", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['type'].should eq("skiplist")
        doc.parsed_response['unique'].should eq(false)
        doc.parsed_response['sparse'].should eq(true)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
        doc.parsed_response['isNewlyCreated'].should eq(true)

        iid = doc.parsed_response['id']

        doc = ArangoDB.log_post("#{prefix}-create-old-skiplist", cmd, :body => body)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['id'].should eq(iid)
        doc.parsed_response['type'].should eq("skiplist")
        doc.parsed_response['unique'].should eq(false)
        doc.parsed_response['sparse'].should eq(true)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
        doc.parsed_response['isNewlyCreated'].should eq(false)
      end

      it "returns either 201 for new or 200 for old indexes, mixed sparsity" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"skiplist\", \"unique\" : false, \"fields\" : [ \"a\", \"b\" ] }"
        doc = ArangoDB.log_post("#{prefix}-create-new-skiplist", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['type'].should eq("skiplist")
        doc.parsed_response['unique'].should eq(false)
        doc.parsed_response['sparse'].should eq(false)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
        doc.parsed_response['isNewlyCreated'].should eq(true)

        iid = doc.parsed_response['id']

        body = "{ \"type\" : \"skiplist\", \"unique\" : false, \"fields\" : [ \"a\", \"b\" ], \"sparse\" : true }"
        doc = ArangoDB.log_post("#{prefix}-create-old-skiplist", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['id'].should_not eq(iid)
        doc.parsed_response['type'].should eq("skiplist")
        doc.parsed_response['unique'].should eq(false)
        doc.parsed_response['sparse'].should eq(true)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
        doc.parsed_response['isNewlyCreated'].should eq(true)
      end
    end

################################################################################
## creating a skiplist and unloading
################################################################################

    context "skiplists after unload/load:" do
      before do
        @cn = "UnitTestsCollectionIndexes"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "survives unload" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"skiplist\", \"unique\" : false, \"fields\" : [ \"a\", \"b\" ] }"
        doc = ArangoDB.post(cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)

        iid = doc.parsed_response['id']

        cmd = api + "/#{iid}"
        doc = ArangoDB.get(cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['id'].should eq(iid)
        doc.parsed_response['type'].should eq("skiplist")
        doc.parsed_response['unique'].should eq(false)
        doc.parsed_response['sparse'].should eq(false)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
      end

      it "survives unload, sparse index" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"skiplist\", \"unique\" : false, \"fields\" : [ \"a\", \"b\" ], \"sparse\" : true }"
        doc = ArangoDB.post(cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)

        iid = doc.parsed_response['id']

        cmd = api + "/#{iid}"
        doc = ArangoDB.get(cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['id'].should eq(iid)
        doc.parsed_response['type'].should eq("skiplist")
        doc.parsed_response['unique'].should eq(false)
        doc.parsed_response['sparse'].should eq(true)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
      end
    end

################################################################################
## creating a unique skiplist
################################################################################

    context "creating unique skiplists:" do
      before do
        @cn = "UnitTestsCollectionIndexes"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "returns either 201 for new or 200 for old indexes" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"skiplist\", \"unique\" : true, \"fields\" : [ \"a\", \"b\" ] }"
        doc = ArangoDB.log_post("#{prefix}-create-new-unique-skiplist", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['type'].should eq("skiplist")
        doc.parsed_response['unique'].should eq(true)
        doc.parsed_response['sparse'].should eq(false)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
        doc.parsed_response['isNewlyCreated'].should eq(true)

        iid = doc.parsed_response['id']

        doc = ArangoDB.log_post("#{prefix}-create-old-unique-skiplist", cmd, :body => body)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['id'].should eq(iid)
        doc.parsed_response['type'].should eq("skiplist")
        doc.parsed_response['unique'].should eq(true)
        doc.parsed_response['sparse'].should eq(false)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
        doc.parsed_response['isNewlyCreated'].should eq(false)
      end

      it "returns either 201 for new or 200 for old indexes, sparse index" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"skiplist\", \"unique\" : true, \"fields\" : [ \"a\", \"b\" ], \"sparse\" : true }"
        doc = ArangoDB.log_post("#{prefix}-create-new-unique-skiplist", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['type'].should eq("skiplist")
        doc.parsed_response['unique'].should eq(true)
        doc.parsed_response['sparse'].should eq(true)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
        doc.parsed_response['isNewlyCreated'].should eq(true)

        iid = doc.parsed_response['id']

        doc = ArangoDB.log_post("#{prefix}-create-old-unique-skiplist", cmd, :body => body)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['id'].should eq(iid)
        doc.parsed_response['type'].should eq("skiplist")
        doc.parsed_response['unique'].should eq(true)
        doc.parsed_response['sparse'].should eq(true)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
        doc.parsed_response['isNewlyCreated'].should eq(false)
      end
    end

################################################################################
## creating a skiplist and unloading
################################################################################

    context "unique skiplists after unload/load:" do
      before do
        @cn = "UnitTestsCollectionIndexes"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "survives unload" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"skiplist\", \"unique\" : true, \"fields\" : [ \"a\", \"b\" ] }"
        doc = ArangoDB.post(cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should_not eq(0)

        iid = doc.parsed_response['id']

        cmd = api + "/#{iid}"
        doc = ArangoDB.get(cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['id'].should eq(iid)
        doc.parsed_response['type'].should eq("skiplist")
        doc.parsed_response['unique'].should eq(true)
        doc.parsed_response['sparse'].should eq(false)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
      end

      it "survives unload, sparse index" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"skiplist\", \"unique\" : true, \"fields\" : [ \"a\", \"b\" ], \"sparse\" : true }"
        doc = ArangoDB.post(cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should_not eq(0)

        iid = doc.parsed_response['id']

        cmd = api + "/#{iid}"
        doc = ArangoDB.get(cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['id'].should eq(iid)
        doc.parsed_response['type'].should eq("skiplist")
        doc.parsed_response['unique'].should eq(true)
        doc.parsed_response['sparse'].should eq(true)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
      end
    end

################################################################################
## reading all indexes
################################################################################

    context "reading all indexes:" do
      before do
        @cn = "UnitTestsCollectionIndexes"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "returns all index for a collection identifier" do
        cmd = api + "?collection=#{@cn}"
        doc = ArangoDB.log_get("#{prefix}-all-indexes", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)

        indexes = doc.parsed_response['indexes']
        identifiers = doc.parsed_response['identifiers']

        for index in indexes do
          index['id'].should match(@reFull)
          identifiers[index['id']].should eq(index)
        end
      end

      it "returns all index for a collection name" do
        cmd = api + "?collection=#{@cn}"
        doc = ArangoDB.log_get("#{prefix}-all-indexes-name", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)

        indexes = doc.parsed_response['indexes']
        identifiers = doc.parsed_response['identifiers']

        for index in indexes do
          index['id'].should match(@reFull)
          identifiers[index['id']].should eq(index)
        end
      end
    end

################################################################################
## reading one index
################################################################################

    context "reading an index:" do
      before do
        @cn = "UnitTestsCollectionIndexes"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "returns primary index for a collection identifier" do
        cmd = api + "/#{@cn}/0"
        doc = ArangoDB.log_get("#{prefix}-primary-index", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq("#{@cn}/0")
        doc.parsed_response['type'].should eq("primary")
      end

      it "returns primary index for a collection name" do
        cmd = api + "/#{@cn}/0"
        doc = ArangoDB.log_get("#{prefix}-primary-index-name", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq("#{@cn}/0")
        doc.parsed_response['type'].should eq("primary")
      end
    end

################################################################################
## deleting an index
################################################################################

    context "deleting an index:" do
      before do
        @cn = "UnitTestsCollectionIndexes"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "deleting an index" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"skiplist\", \"unique\" : true, \"fields\" : [ \"a\", \"b\" ] }"
        doc = ArangoDB.post(cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['type'].should eq("skiplist")
        doc.parsed_response['unique'].should eq(true)
        doc.parsed_response['sparse'].should eq(false)
        doc.parsed_response['fields'].should eq([ "a", "b" ])
        doc.parsed_response['isNewlyCreated'].should eq(true)

        iid = doc.parsed_response['id']

        cmd = api + "/#{iid}"
        doc = ArangoDB.log_delete("#{prefix}-delete-unique-skiplist", cmd)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['id'].should eq(iid)

        cmd = api + "/#{iid}"
        doc = ArangoDB.get(cmd)

        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
        doc.parsed_response['errorNum'].should eq(1212)
      end
    end

  end
end
