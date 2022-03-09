# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_api/index"
  prefix = "api-index"

  context "dealing with geo indexes:" do
    before do
      @reFull = Regexp.new('^[a-zA-Z0-9_\-]+/\d+$')
    end

################################################################################
## creating a geo index
################################################################################

    context "creating geo indexes:" do
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
        body = "{ \"type\" : \"geo\", \"fields\" : [ \"a\" ] }"
        doc = ArangoDB.log_post("#{prefix}-create-new-geo", cmd, :body => body)
  
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['type'].should eq("geo")
        doc.parsed_response['geoJson'].should eq(false)
        doc.parsed_response['fields'].should eq([ "a" ])
        doc.parsed_response['isNewlyCreated'].should eq(true)
        doc.parsed_response['unique'].should eq(false)
        doc.parsed_response['sparse'].should eq(true)

        doc = ArangoDB.log_post("#{prefix}-create-old-geo", cmd, :body => body)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['type'].should eq("geo")
        doc.parsed_response['geoJson'].should eq(false)
        doc.parsed_response['fields'].should eq([ "a" ])
        doc.parsed_response['isNewlyCreated'].should eq(false)
        doc.parsed_response['unique'].should eq(false)
        doc.parsed_response['sparse'].should eq(true)
      end

      it "creating geo index with location" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"geo\", \"fields\" : [ \"b\" ] }"
        doc = ArangoDB.log_post("#{prefix}-create-geo-location", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['type'].should eq("geo")
        doc.parsed_response['geoJson'].should eq(false)
        doc.parsed_response['fields'].should eq([ "b" ])
        doc.parsed_response['isNewlyCreated'].should eq(true)
        doc.parsed_response['unique'].should eq(false)
        doc.parsed_response['sparse'].should eq(true)
      end

      it "creating geo index with location and geo-json = true" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"geo\", \"fields\" : [ \"c\" ], \"geoJson\" : true }"
        doc = ArangoDB.log_post("#{prefix}-create-geo-location-geo-json", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['type'].should eq("geo")
        doc.parsed_response['geoJson'].should eq(true)
        doc.parsed_response['fields'].should eq([ "c" ])
        doc.parsed_response['isNewlyCreated'].should eq(true)
        doc.parsed_response['unique'].should eq(false)
        doc.parsed_response['sparse'].should eq(true)
      end

      it "creating geo index with location and geo-json = false" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"geo\", \"fields\" : [ \"d\" ], \"geoJson\" : false }"
        doc = ArangoDB.log_post("#{prefix}-create-geo-location-no-geo-json", cmd, :body => body)
  
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['type'].should eq("geo")
        doc.parsed_response['geoJson'].should eq(false)
        doc.parsed_response['fields'].should eq([ "d" ])
        doc.parsed_response['isNewlyCreated'].should eq(true)
        doc.parsed_response['unique'].should eq(false)
        doc.parsed_response['sparse'].should eq(true)
      end

      it "creating geo index with latitude and longitude" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"geo\", \"fields\" : [ \"e\", \"f\" ] }"
        doc = ArangoDB.log_post("#{prefix}-create-geo-latitude-longitude", cmd, :body => body)
  
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['type'].should eq("geo")
        doc.parsed_response['fields'].should eq([ "e", "f" ])
        doc.parsed_response['isNewlyCreated'].should eq(true)
        doc.parsed_response['unique'].should eq(false)
        doc.parsed_response['sparse'].should eq(true)
      end

      it "creating geo index with constraint 1" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"geo\", \"fields\" : [ \"c\" ], \"geoJson\" : true, \"constraint\" : true }"
        doc = ArangoDB.log_post("#{prefix}-create-geo-location-geo-json-constraint", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['type'].should eq("geo")
        doc.parsed_response['geoJson'].should eq(true)
        doc.parsed_response['fields'].should eq([ "c" ])
        doc.parsed_response['unique'].should eq(false)
        doc.parsed_response['isNewlyCreated'].should eq(true)
        doc.parsed_response['sparse'].should eq(true)
      end
      
      it "creating geo index with constraint 2" do
        cmd = api + "?collection=#{@cn}"
        body = "{ \"type\" : \"geo\", \"fields\" : [ \"c\", \"d\" ], \"geoJson\" : false, \"unique\" : true }"
        doc = ArangoDB.log_post("#{prefix}-create-geo-location-constraint", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should match(@reFull)
        doc.parsed_response['type'].should eq("geo")
        doc.parsed_response['fields'].should eq([ "c", "d" ])
        doc.parsed_response['unique'].should eq(false)
        doc.parsed_response['isNewlyCreated'].should eq(true)
        doc.parsed_response['sparse'].should eq(true)
      end
    end

################################################################################
## creating a geo index and unloading
################################################################################

    context "geo indexes after unload/load:" do
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
        body = "{ \"type\" : \"geo\", \"fields\" : [ \"a\" ] }"
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
        doc.parsed_response['type'].should eq("geo")
        doc.parsed_response['geoJson'].should eq(false)
        doc.parsed_response['fields'].should eq([ "a" ])
        doc.parsed_response['sparse'].should eq(true)
      end
    end

  end
end
