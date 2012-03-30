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
## creating a geo index
################################################################################

    context "geo indexes:" do
      before do
	@cn = "UnitTestsCollectionIndexes"
	AvocadoDB.drop_collection(@cn)
	@cid = AvocadoDB.create_collection(@cn)
      end

      after do
	AvocadoDB.drop_collection(@cn)
    end

      it "returns either 201 for new or 200 for old indexes" do
	cmd = api + "/#{@cid}"
	body = "{ \"type\" : \"geo\", \"fields\" : [ \"a\" ] }"
        doc = AvocadoDB.log_post("#{prefix}-create-new-geo", cmd, :body => body)
	
	doc.code.should eq(201)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(201)
	doc.parsed_response['id'].should_not eq(0)
	doc.parsed_response['type'].should eq("geo")
	doc.parsed_response['geoJson'].should eq(false)
	doc.parsed_response['fields'].should eq([ "a" ])
	doc.parsed_response['isNewlyCreated'].should eq(true)

        doc = AvocadoDB.log_post("#{prefix}-create-old-geo", cmd, :body => body)
	
	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should_not eq(0)
	doc.parsed_response['type'].should eq("geo")
	doc.parsed_response['geoJson'].should eq(false)
	doc.parsed_response['fields'].should eq([ "a" ])
	doc.parsed_response['isNewlyCreated'].should eq(false)
      end

      it "creating geo index with location" do
	cmd = api + "/#{@cid}"
	body = "{ \"type\" : \"geo\", \"fields\" : [ \"b\" ] }"
        doc = AvocadoDB.log_post("#{prefix}-create-geo-location", cmd, :body => body)
	
	doc.code.should eq(201)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(201)
	doc.parsed_response['id'].should_not eq(0)
	doc.parsed_response['type'].should eq("geo")
	doc.parsed_response['geoJson'].should eq(false)
	doc.parsed_response['fields'].should eq([ "b" ])
	doc.parsed_response['isNewlyCreated'].should eq(true)
      end

      it "creating geo index with location and geo-json = true" do
	cmd = api + "/#{@cid}"
	body = "{ \"type\" : \"geo\", \"fields\" : [ \"c\" ], \"geoJson\" : true }"
        doc = AvocadoDB.log_post("#{prefix}-create-geo-location-geo-json", cmd, :body => body)
	
	doc.code.should eq(201)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(201)
	doc.parsed_response['id'].should_not eq(0)
	doc.parsed_response['type'].should eq("geo")
	doc.parsed_response['geoJson'].should eq(true)
	doc.parsed_response['fields'].should eq([ "c" ])
	doc.parsed_response['isNewlyCreated'].should eq(true)
      end

      it "creating geo index with location and geo-json = false" do
	cmd = api + "/#{@cid}"
	body = "{ \"type\" : \"geo\", \"fields\" : [ \"d\" ], \"geoJson\" : false }"
        doc = AvocadoDB.log_post("#{prefix}-create-geo-location-no-geo-json", cmd, :body => body)
	
	doc.code.should eq(201)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(201)
	doc.parsed_response['id'].should_not eq(0)
	doc.parsed_response['type'].should eq("geo")
	doc.parsed_response['geoJson'].should eq(false)
	doc.parsed_response['fields'].should eq([ "d" ])
	doc.parsed_response['isNewlyCreated'].should eq(true)
      end

      it "creating geo index with latitude and longitude" do
	cmd = api + "/#{@cid}"
	body = "{ \"type\" : \"geo\", \"fields\" : [ \"e\", \"f\" ] }"
        doc = AvocadoDB.log_post("#{prefix}-create-geo-latitude-longitude", cmd, :body => body)
	
	doc.code.should eq(201)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(201)
	doc.parsed_response['id'].should_not eq(0)
	doc.parsed_response['type'].should eq("geo")
	doc.parsed_response['fields'].should eq([ "e", "f" ])
	doc.parsed_response['isNewlyCreated'].should eq(true)
      end
    end

################################################################################
## creating a geo index and unloading
################################################################################

    context "geo indexes after unload/load:" do
      before do
	@cn = "UnitTestsCollectionIndexes"
	AvocadoDB.drop_collection(@cn)
	@cid = AvocadoDB.create_collection(@cn)
      end

      after do
	AvocadoDB.drop_collection(@cn)
    end

      it "survives" do
	cmd = api + "/#{@cid}"
	body = "{ \"type\" : \"geo\", \"fields\" : [ \"a\" ] }"
        doc = AvocadoDB.post(cmd, :body => body)
	
	doc.code.should eq(201)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(201)
	doc.parsed_response['id'].should_not eq(0)

	iid = doc.parsed_response['id']

	cmd = "/_api/collection/#{@cid}/unload"
	doc = AvocadoDB.put(cmd)

	doc.code.should eq(200)

	cmd = "/_api/collection/#{@cid}"
	doc = AvocadoDB.get(cmd)
	doc.code.should eq(200)

	while doc.parsed_response['status'] != 2
	  doc = AvocadoDB.get(cmd)
	  doc.code.should eq(200)
	end

	cmd = api + "/#{@cid}/#{iid}"
        doc = AvocadoDB.get(cmd, :body => body)

	doc.code.should eq(200)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(200)
	doc.parsed_response['id'].should eq(iid)
	doc.parsed_response['type'].should eq("geo")
	doc.parsed_response['geoJson'].should eq(false)
	doc.parsed_response['fields'].should eq([ "a" ])
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
