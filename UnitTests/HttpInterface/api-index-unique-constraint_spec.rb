require 'rspec'
require './avocadodb.rb'

describe AvocadoDB do
  api = "/_api/index"
  prefix = "api-index-unique-constraint"

  context "one attribute:" do
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
      
	cmd1 = "/document?collection=#{@cid}"

	# create a document
	body = "{ \"a\" : 1, \"b\" : 1 }"
	doc = AvocadoDB.log_post("#{prefix}", cmd1, :body => body)

	doc.code.should eq(201)

	id1 = doc.parsed_response['_id']
	id1.should be_kind_of(String)

	rev1 = doc.parsed_response['_rev']
	rev1.should be_kind_of(Integer)

	cmd2 = "/document/#{id1}"

	# check it
	doc = AvocadoDB.log_get("#{prefix}", cmd2)

	doc.code.should eq(200)
	doc.parsed_response['a'].should eq(1)
	doc.parsed_response['b'].should eq(1)

	# create a unique constraint violation
	body = "{ \"a\" : 1, \"b\" : 2 }"
	doc = AvocadoDB.log_post("#{prefix}", cmd1, :body => body)

	doc.code.should eq(409)

	# id2 = doc.parsed_response['_id']
	# id2.should be_kind_of(String)

	# rev2 = doc.parsed_response['_rev']
	# rev2.should be_kind_of(Integer)

	# check the old document again
	doc = AvocadoDB.log_get("#{prefix}", cmd2)

	doc.code.should eq(200)
	doc.parsed_response['a'].should eq(1)
	doc.parsed_response['b'].should eq(1)

	#
	body = "{ \"a\" : 1, \"b\" : 3 }"
	doc = AvocadoDB.log_post("#{prefix}", cmd1, :body => body)

	doc.code.should eq(409)

	# id3 = doc.parsed_response['_id']
	# id3.should be_kind_of(String)

	# rev3 = doc.parsed_response['_rev']
	# rev3.should be_kind_of(Integer)

	body = "{ \"a\" : 1 }"
	doc = AvocadoDB.log_post("#{prefix}", cmd1, :body => body)
      end
    end
  end
end
