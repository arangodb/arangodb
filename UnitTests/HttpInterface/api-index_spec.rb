require 'rspec'
require './avocadodb.rb'

describe AvocadoDB do
  api = "/_api/index"
  prefix = "api-index"

  context "dealing with indexes:" do

################################################################################
## error handling
################################################################################

    context "error handling:" do
      before do
	@cn = "UnitTestsCollectionIndexes"
	AvocadoDB.drop_collection(@cn)
	@cid = AvocadoDB.create_collection(@cn)
      end

      after do
	AvocadoDB.drop_collection(@cn)
      end

      it "returns an error if collection identifier is unknown" do
	cmd = api + "/123456/123456"
        doc = AvocadoDB.log_get("#{prefix}-bad-collection-identifier", cmd)

	doc.code.should eq(404)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1203)
	doc.parsed_response['code'].should eq(404)
      end

      it "returns an error if index identifier is unknown" do
	cmd = api + "/#{@cid}/123456"
        doc = AvocadoDB.log_get("#{prefix}-bad-index-identifier", cmd)

	doc.code.should eq(404)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['errorNum'].should eq(1212)
	doc.parsed_response['code'].should eq(404)
      end
    end

################################################################################
## reading all indexes
################################################################################

    context "all indexes:" do
      before do
	@cn = "UnitTestsCollectionIndexes"
	AvocadoDB.drop_collection(@cn)
	@cid = AvocadoDB.create_collection(@cn)
      end

      after do
	AvocadoDB.drop_collection(@cn)
      end

      it "returns all index for an collection identifier" do
	cmd = api + "/#{@cid}"
        doc = AvocadoDB.log_get("#{prefix}-all-indexes", cmd)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)

	indexes = doc.parsed_response['indexes']
	identifiers = doc.parsed_response['identifiers']

	for index in indexes do
	  indexes[index['id']].should eq(index)
	end
      end

      it "returns all index for an collection name" do
	cmd = api + "/#{@cn}"
        doc = AvocadoDB.log_get("#{prefix}-all-indexes-name", cmd)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)

	indexes = doc.parsed_response['indexes']
	identifiers = doc.parsed_response['identifiers']

	for index in indexes do
	  indexes[index['id']].should eq(index)
	end
      end
    end

################################################################################
## reading one index
################################################################################

    context "reading an index:" do
      before do
	@cn = "UnitTestsCollectionIndexes"
	AvocadoDB.drop_collection(@cn)
	@cid = AvocadoDB.create_collection(@cn)
      end

      after do
	AvocadoDB.drop_collection(@cn)
      end

      it "returns primary index for an collection identifier" do
	cmd = api + "/#{@cid}/0"
        doc = AvocadoDB.log_get("#{prefix}-primary-index", cmd)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(0)
	doc.parsed_response['type'].should eq("primary")
      end

      it "returns primary index for an collection name" do
	cmd = api + "/#{@cn}/0"
        doc = AvocadoDB.log_get("#{prefix}-primary-index-name", cmd)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(0)
	doc.parsed_response['type'].should eq("primary")
      end
    end

  end
end
