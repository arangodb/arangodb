# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do
  api = "/_api/index"
  prefix = "api-index-hash"

################################################################################
## unique constraints during create
################################################################################

  context "creating hash index:" do
    context "dealing with unique constraints violation:" do
      before do
        @cn = "UnitTestsCollectionIndexes"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn)
      end
      
      after do
        ArangoDB.drop_collection(@cn)
      end
      
      it "does not create the index in case of violation" do
        # create a document
        cmd1 = "/_api/document?collection=#{@cn}"
        body = "{ \"a\" : 1, \"b\" : 1 }"
        doc = ArangoDB.log_post("#{prefix}-create2", cmd1, :body => body)

        doc.code.should eq(201)

        # create another document
        cmd1 = "/_api/document?collection=#{@cn}"
        body = "{ \"a\" : 1, \"b\" : 1 }"
        doc = ArangoDB.log_post("#{prefix}-create2", cmd1, :body => body)

        doc.code.should eq(201)

        # try to create the index
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"hash\", \"unique\" : true, \"fields\" : [ \"a\" ] }"
        doc = ArangoDB.log_post("#{prefix}-fail", cmd, :body => body)

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1210)
      end
    end
  end

################################################################################
## unique constraints during create
################################################################################

  context "creating documents:" do
    context "dealing with unique constraints:" do
      before do
        @cn = "UnitTestsCollectionIndexes"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn)
      end
      
      after do
        ArangoDB.drop_collection(@cn)
      end
      
      it "rolls back in case of violation" do
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"hash\", \"unique\" : true, \"fields\" : [ \"a\" ] }"
        doc = ArangoDB.log_post("#{prefix}-create1", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['type'].should eq("hash")
        doc.parsed_response['unique'].should eq(true)
            
        # create a document
        cmd1 = "/_api/document?collection=#{@cn}"
        body = "{ \"a\" : 1, \"b\" : 1 }"
        doc = ArangoDB.log_post("#{prefix}-create2", cmd1, :body => body)

        doc.code.should eq(201)

        id1 = doc.parsed_response['_id']
        id1.should be_kind_of(String)

        rev1 = doc.parsed_response['_rev']
        rev1.should be_kind_of(String)

        # check it
        cmd2 = "/_api/document/#{id1}"
        doc = ArangoDB.log_get("#{prefix}", cmd2)

        doc.code.should eq(200)
        doc.parsed_response['a'].should eq(1)
        doc.parsed_response['b'].should eq(1)
        doc.parsed_response['_id'].should eq(id1)
        doc.parsed_response['_rev'].should eq(rev1)

        # create a unique constraint violation
        body = "{ \"a\" : 1, \"b\" : 2 }"
        doc = ArangoDB.log_post("#{prefix}-create3", cmd1, :body => body)

        doc.code.should eq(409)

        # check it again
        doc = ArangoDB.log_get("#{prefix}", cmd2)

        doc.code.should eq(200)
        doc.parsed_response['a'].should eq(1)
        doc.parsed_response['b'].should eq(1)
        doc.parsed_response['_id'].should eq(id1)
        doc.parsed_response['_rev'].should eq(rev1)

        # third try (make sure the rollback has not destroyed anything)
        body = "{ \"a\" : 1, \"b\" : 3 }"
        doc = ArangoDB.log_post("#{prefix}-create4", cmd1, :body => body)

        doc.code.should eq(409)

        # check it again
        doc = ArangoDB.log_get("#{prefix}", cmd2)

        doc.code.should eq(200)
        doc.parsed_response['a'].should eq(1)
        doc.parsed_response['b'].should eq(1)
        doc.parsed_response['_id'].should eq(id1)
        doc.parsed_response['_rev'].should eq(rev1)

        # unload collection
        cmd3 = "/_api/collection/#{@cn}/unload"
        doc = ArangoDB.log_put("#{prefix}", cmd3)

        doc.code.should eq(200)

        cmd3 = "/_api/collection/#{@cn}"
        doc = ArangoDB.log_get("#{prefix}", cmd3)
        doc.code.should eq(200)

        while doc.parsed_response['status'] != 2
          doc = ArangoDB.get(cmd3)
          doc.code.should eq(200)
        end

        # check it again
        doc = ArangoDB.log_get("#{prefix}", cmd2)

        doc.code.should eq(200)
        doc.parsed_response['a'].should eq(1)
        doc.parsed_response['b'].should eq(1)
        doc.parsed_response['_id'].should eq(id1)
        doc.parsed_response['_rev'].should eq(rev1)
      end
    end
  end

################################################################################
## unique constraints during update
################################################################################

  context "updating documents:" do
    context "dealing with unique constraints:" do
      before do
        @cn = "UnitTestsCollectionIndexes"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn)
      end
      
      after do
        ArangoDB.drop_collection(@cn)
      end
      
      it "rolls back in case of violation" do
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"hash\", \"unique\" : true, \"fields\" : [ \"a\" ] }"
        doc = ArangoDB.log_post("#{prefix}-update1", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['type'].should eq("hash")
        doc.parsed_response['unique'].should eq(true)
            
        # create a document
        cmd1 = "/_api/document?collection=#{@cn}"
        body = "{ \"a\" : 1, \"b\" : 1 }"
        doc = ArangoDB.log_post("#{prefix}-update2", cmd1, :body => body)

        doc.code.should eq(201)

        id1 = doc.parsed_response['_id']
        id1.should be_kind_of(String)

        rev1 = doc.parsed_response['_rev']
        rev1.should be_kind_of(String)

        # check it
        cmd2 = "/_api/document/#{id1}"
        doc = ArangoDB.log_get("#{prefix}", cmd2)

        doc.code.should eq(200)
        doc.parsed_response['a'].should eq(1)
        doc.parsed_response['b'].should eq(1)
        doc.parsed_response['_id'].should eq(id1)
        doc.parsed_response['_rev'].should eq(rev1)

        # create a second document
        body = "{ \"a\" : 2, \"b\" : 2 }"
        doc = ArangoDB.log_post("#{prefix}-update3", cmd1, :body => body)

        doc.code.should eq(201)

        id2 = doc.parsed_response['_id']
        id2.should be_kind_of(String)

        rev2 = doc.parsed_response['_rev']
        rev2.should be_kind_of(String)

        # create a unique constraint violation during update
        body = "{ \"a\" : 2, \"b\" : 3 }"
        doc = ArangoDB.log_put("#{prefix}", cmd2, :body => body)

        doc.code.should eq(409)

        # check first document again
        doc = ArangoDB.log_get("#{prefix}", cmd2)

        doc.code.should eq(200)
        doc.parsed_response['a'].should eq(1)
        doc.parsed_response['b'].should eq(1)
        doc.parsed_response['_id'].should eq(id1)
        doc.parsed_response['_rev'].should eq(rev1)

        rev3 = doc.parsed_response['_rev']
        rev3.should be_kind_of(String)

        # check second document again
        cmd3 = "/_api/document/#{id2}"
        doc = ArangoDB.log_get("#{prefix}", cmd3)

        doc.code.should eq(200)
        doc.parsed_response['a'].should eq(2)
        doc.parsed_response['b'].should eq(2)
        doc.parsed_response['_id'].should eq(id2)
        doc.parsed_response['_rev'].should eq(rev2)

        # third try (make sure the rollback has not destroyed anything)
        body = "{ \"a\" : 2, \"b\" : 4 }"
        doc = ArangoDB.log_put("#{prefix}", cmd2, :body => body)

        doc.code.should eq(409)

        # check the first document again
        doc = ArangoDB.log_get("#{prefix}", cmd2)

        doc.code.should eq(200)
        doc.parsed_response['a'].should eq(1)
        doc.parsed_response['b'].should eq(1)
        doc.parsed_response['_id'].should eq(id1)
        doc.parsed_response['_rev'].should eq(rev1)
        doc.parsed_response['_rev'].should_not eq(rev2)

        # unload collection
        cmd4 = "/_api/collection/#{@cn}/unload"
        doc = ArangoDB.log_put("#{prefix}", cmd4)

        doc.code.should eq(200)

        cmd4 = "/_api/collection/#{@cn}"
        doc = ArangoDB.log_get("#{prefix}", cmd4)
        doc.code.should eq(200)

        while doc.parsed_response['status'] != 2
          doc = ArangoDB.get(cmd4)
          doc.code.should eq(200)
        end

        # check the first document again
        doc = ArangoDB.log_get("#{prefix}", cmd2)

        doc.code.should eq(200)
        doc.parsed_response['a'].should eq(1)
        doc.parsed_response['b'].should eq(1)
        doc.parsed_response['_id'].should eq(id1)
        doc.parsed_response['_rev'].should eq(rev1)
        doc.parsed_response['_rev'].should_not eq(rev2)
      end
    end

  end
end
