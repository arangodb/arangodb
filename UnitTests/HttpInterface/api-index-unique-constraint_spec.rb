require 'rspec'
require './avocadodb.rb'

describe AvocadoDB do
  api = "/_api/index"
  prefix = "api-index-unique-constraint"

################################################################################
## unique constraints during create
################################################################################

  context "creating:" do
    context "dealing with unique constraints:" do
      before do
	@cn = "UnitTestsCollectionIndexes"
	AvocadoDB.drop_collection(@cn)
	@cid = AvocadoDB.create_collection(@cn)
      end
      
      after do
	AvocadoDB.drop_collection(@cn)
      end
      
      it "rolls back in case of violation" do
	cmd = "/_api/index/#{@cid}"
	body = "{ \"type\" : \"hash\", \"unique\" : true, \"fields\" : [ \"a\" ] }"
	doc = AvocadoDB.log_post("#{prefix}", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['type'].should eq("hash")
	doc.parsed_response['unique'].should eq(true)
      
	# create a document
	cmd1 = "/document?collection=#{@cid}"
	body = "{ \"a\" : 1, \"b\" : 1 }"
	doc = AvocadoDB.log_post("#{prefix}", cmd1, :body => body)

	doc.code.should eq(201)

	id1 = doc.parsed_response['_id']
	id1.should be_kind_of(String)

	rev1 = doc.parsed_response['_rev']
	rev1.should be_kind_of(Integer)

	# check it
	cmd2 = "/document/#{id1}"
	doc = AvocadoDB.log_get("#{prefix}", cmd2)

	doc.code.should eq(200)
	doc.parsed_response['a'].should eq(1)
	doc.parsed_response['b'].should eq(1)
	doc.parsed_response['_id'].should eq(id1)
	doc.parsed_response['_rev'].should eq(rev1)

	# create a unique constraint violation
	body = "{ \"a\" : 1, \"b\" : 2 }"
	doc = AvocadoDB.log_post("#{prefix}", cmd1, :body => body)

	doc.code.should eq(409)

	# check it again
	doc = AvocadoDB.log_get("#{prefix}", cmd2)

	doc.code.should eq(200)
	doc.parsed_response['a'].should eq(1)
	doc.parsed_response['b'].should eq(1)
	doc.parsed_response['_id'].should eq(id1)
	doc.parsed_response['_rev'].should eq(rev1)

	# third try (make sure the rollback has not destroyed anything)
	body = "{ \"a\" : 1, \"b\" : 3 }"
	doc = AvocadoDB.log_post("#{prefix}", cmd1, :body => body)

	doc.code.should eq(409)

	# check it again
	doc = AvocadoDB.log_get("#{prefix}", cmd2)

	doc.code.should eq(200)
	doc.parsed_response['a'].should eq(1)
	doc.parsed_response['b'].should eq(1)
	doc.parsed_response['_id'].should eq(id1)
	doc.parsed_response['_rev'].should eq(rev1)

	# unload collection
	cmd3 = "/_api/collection/#{@cid}/unload"
	doc = AvocadoDB.log_put("#{prefix}", cmd3)

	doc.code.should eq(200)

	cmd3 = "/_api/collection/#{@cid}"
	doc = AvocadoDB.log_get("#{prefix}", cmd3)
	doc.code.should eq(200)

	while doc.parsed_response['status'] != 2
	  doc = AvocadoDB.get(cmd3)
	  doc.code.should eq(200)
	end

	# check it again
	doc = AvocadoDB.log_get("#{prefix}", cmd2)

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

  context "updating:" do
    context "dealing with unique constraints:" do
      before do
	@cn = "UnitTestsCollectionIndexes"
	AvocadoDB.drop_collection(@cn)
	@cid = AvocadoDB.create_collection(@cn)
      end
      
      after do
	AvocadoDB.drop_collection(@cn)
      end
      
      it "rolls back in case of violation" do
	cmd = "/_api/index/#{@cid}"
	body = "{ \"type\" : \"hash\", \"unique\" : true, \"fields\" : [ \"a\" ] }"
	doc = AvocadoDB.log_post("#{prefix}", cmd, :body => body)

	doc.code.should eq(201)
	doc.parsed_response['type'].should eq("hash")
	doc.parsed_response['unique'].should eq(true)
      
	# create a document
	cmd1 = "/document?collection=#{@cid}"
	body = "{ \"a\" : 1, \"b\" : 1 }"
	doc = AvocadoDB.log_post("#{prefix}", cmd1, :body => body)

	doc.code.should eq(201)

	id1 = doc.parsed_response['_id']
	id1.should be_kind_of(String)

	rev1 = doc.parsed_response['_rev']
	rev1.should be_kind_of(Integer)

	# check it
	cmd2 = "/document/#{id1}"
	doc = AvocadoDB.log_get("#{prefix}", cmd2)

	doc.code.should eq(200)
	doc.parsed_response['a'].should eq(1)
	doc.parsed_response['b'].should eq(1)
	doc.parsed_response['_id'].should eq(id1)
	doc.parsed_response['_rev'].should eq(rev1)

	# create a second document
	body = "{ \"a\" : 2, \"b\" : 2 }"
	doc = AvocadoDB.log_post("#{prefix}", cmd1, :body => body)

	doc.code.should eq(201)

	id2 = doc.parsed_response['_id']
	id2.should be_kind_of(String)

	rev2 = doc.parsed_response['_rev']
	rev2.should be_kind_of(Integer)

	# create a unique constraint violation during update
	body = "{ \"a\" : 2, \"b\" : 3 }"
	doc = AvocadoDB.log_put("#{prefix}", cmd2, :body => body)

	doc.code.should eq(409)

	# check first document again
	doc = AvocadoDB.log_get("#{prefix}", cmd2)

	doc.code.should eq(200)
	doc.parsed_response['a'].should eq(1)
	doc.parsed_response['b'].should eq(1)
	doc.parsed_response['_id'].should eq(id1)
	doc.parsed_response['_rev'].should_not eq(rev1)

	rev3 = doc.parsed_response['_rev']
	rev3.should be_kind_of(Integer)

	# check second document again
	cmd3 = "/document/#{id2}"
	doc = AvocadoDB.log_get("#{prefix}", cmd3)

	doc.code.should eq(200)
	doc.parsed_response['a'].should eq(2)
	doc.parsed_response['b'].should eq(2)
	doc.parsed_response['_id'].should eq(id2)
	doc.parsed_response['_rev'].should eq(rev2)

	# third try (make sure the rollback has not destroyed anything)
	body = "{ \"a\" : 2, \"b\" : 4 }"
	doc = AvocadoDB.log_put("#{prefix}", cmd2, :body => body)

	doc.code.should eq(409)

	# check the first document again
	doc = AvocadoDB.log_get("#{prefix}", cmd2)

	doc.code.should eq(200)
	doc.parsed_response['a'].should eq(1)
	doc.parsed_response['b'].should eq(1)
	doc.parsed_response['_id'].should eq(id1)
	doc.parsed_response['_rev'].should_not eq(rev1)
	doc.parsed_response['_rev'].should_not eq(rev3)

	# unload collection
	cmd4 = "/_api/collection/#{@cid}/unload"
	doc = AvocadoDB.log_put("#{prefix}", cmd4)

	doc.code.should eq(200)

	cmd4 = "/_api/collection/#{@cid}"
	doc = AvocadoDB.log_get("#{prefix}", cmd4)
	doc.code.should eq(200)

	while doc.parsed_response['status'] != 2
	  doc = AvocadoDB.get(cmd4)
	  doc.code.should eq(200)
	end

	# check the first document again
	doc = AvocadoDB.log_get("#{prefix}", cmd2)

	doc.code.should eq(200)
	doc.parsed_response['a'].should eq(1)
	doc.parsed_response['b'].should eq(1)
	doc.parsed_response['_id'].should eq(id1)
	doc.parsed_response['_rev'].should_not eq(rev1)
	doc.parsed_response['_rev'].should_not eq(rev3)
      end
    end

  end
end
