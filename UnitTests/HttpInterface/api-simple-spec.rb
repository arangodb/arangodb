# coding: utf-8

require 'rspec'
require './avocadodb.rb'

describe AvocadoDB do
  api = "/_api/simple"
  prefix = "api-simple"

  context "simple queries:" do

################################################################################
## all query
################################################################################

    context "all query:" do
      before do
	@cn = "UnitTestsCollectionSimple"
	AvocadoDB.drop_collection(@cn)
	@cid = AvocadoDB.create_collection(@cn, false)

	(0...3000).each{|i|
	  AvocadoDB.post("/document?collection=#{@cid}", :body => "{ \"n\" : #{i} }")
	}
      end

      after do
	AvocadoDB.drop_collection(@cn)
      end

      it "get all documents" do
	cmd = api + "/all"
	body = "{ \"collection\" : \"#{@cid}\" }"
	doc = AvocadoDB.log_put("#{prefix}-all", cmd, :body => body)

	doc.code.should eq(201)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(201)
	doc.parsed_response['hasMore'].should eq(true)
	doc.parsed_response['result'].length.should eq(1000)
	doc.parsed_response['count'].should eq(3000)
      end

      it "get all documents with limit" do
	cmd = api + "/all"
	body = "{ \"collection\" : \"#{@cid}\", \"limit\" : 100 }"
	doc = AvocadoDB.log_put("#{prefix}-all-limit", cmd, :body => body)

	doc.code.should eq(201)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(201)
	doc.parsed_response['hasMore'].should eq(false)
	doc.parsed_response['result'].length.should eq(100)
	doc.parsed_response['count'].should eq(100)
      end

      it "get all documents with negative limit" do
	cmd = api + "/all"
	body = "{ \"collection\" : \"#{@cid}\", \"limit\" : -100 }"
	doc = AvocadoDB.log_put("#{prefix}-all-negative-limit", cmd, :body => body)

	doc.code.should eq(201)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(201)
	doc.parsed_response['hasMore'].should eq(false)
	doc.parsed_response['result'].length.should eq(100)
	doc.parsed_response['count'].should eq(100)
      end

      it "get all documents with skip" do
	cmd = api + "/all"
	body = "{ \"collection\" : \"#{@cid}\", \"skip\" : 2900 }"
	doc = AvocadoDB.log_put("#{prefix}-all-skip", cmd, :body => body)

	doc.code.should eq(201)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(201)
	doc.parsed_response['hasMore'].should eq(false)
	doc.parsed_response['result'].length.should eq(100)
	doc.parsed_response['count'].should eq(100)
      end

      it "get all documents with skip and limit" do
	cmd = api + "/all"
	body = "{ \"collection\" : \"#{@cid}\", \"skip\" : 2900, \"limit\" : 2 }"
	doc = AvocadoDB.log_put("#{prefix}-all-skip-limit", cmd, :body => body)

	doc.code.should eq(201)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(201)
	doc.parsed_response['hasMore'].should eq(false)
	doc.parsed_response['result'].length.should eq(2)
	doc.parsed_response['count'].should eq(2)
      end
    end

################################################################################
## geo near query
################################################################################

    context "geo query:" do
      before do
	@cn = "UnitTestsCollectionGeo"
	AvocadoDB.drop_collection(@cn)
	@cid = AvocadoDB.create_collection(@cn, false)

	cmd = api + "?collection=#{@cid}"
	body = "{ \"type\" : \"geo\", \"fields\" : [ \"a\" ] }"
        #doc = AvocadoDB.post(cmd, :body => body)

	(0..10).each{|i|
	  lat = 10 * (i - 5)

	  (0..10).each{|j|
	    lon = 10 * (j - 5)
	    
	    AvocadoDB.post("/document?collection=#{@cid}", :body => "{ \"loc\" : [ #{lat}, #{lon} ] }")
	  }
	}
      end

      after do
	AvocadoDB.drop_collection(@cn)
      end

      it "returns an error for near without index" do
	cmd = api + "/near"
	body = "{ \"collection\" : \"#{@cid}\", \"latitude\" : 0, \"longitude\" : 0, \"skip\" : 1, \"limit\" : 5 }"
	doc = AvocadoDB.log_put("#{prefix}-near-missing", cmd, :body => body)

	doc.code.should eq(400)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)
	doc.parsed_response['errorNum'].should eq(1512)
      end

      it "returns documents near a point" do
	cmd = "/_api/index?collection=#{@cid}"
	body = "{ \"type\" : \"geo\", \"fields\" : [ \"loc\" ] }"
        doc = AvocadoDB.post(cmd, :body => body)

	cmd = api + "/near"
	body = "{ \"collection\" : \"#{@cid}\", \"latitude\" : 0, \"longitude\" : 0, \"skip\" : 1, \"limit\" : 5 }"
	doc = AvocadoDB.log_put("#{prefix}-near", cmd, :body => body)

	doc.code.should eq(201)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(201)
	doc.parsed_response['hasMore'].should eq(false)
	doc.parsed_response['result'].length.should eq(5)
	doc.parsed_response['count'].should eq(5)
      end

      it "returns documents and distance near a point" do
	cmd = "/_api/index?collection=#{@cid}"
	body = "{ \"type\" : \"geo\", \"fields\" : [ \"loc\" ] }"
        doc = AvocadoDB.post(cmd, :body => body)

	cmd = api + "/near"
	body = "{ \"collection\" : \"#{@cid}\", \"latitude\" : 0, \"longitude\" : 0, \"limit\" : 5, \"distance\" : \"distance\" }"
	doc = AvocadoDB.log_put("#{prefix}-near-distance", cmd, :body => body)

	doc.code.should eq(201)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(201)
	doc.parsed_response['hasMore'].should eq(false)
	doc.parsed_response['result'].length.should eq(5)
	doc.parsed_response['count'].should eq(5)

	doc.parsed_response['result'].map{|i| i['distance'].floor.to_f}.should eq([0,1111949,1111949,1111949,1111949])
      end
    end

################################################################################
## geo within query
################################################################################

    context "geo query:" do
      before do
	@cn = "UnitTestsCollectionGeo"
	AvocadoDB.drop_collection(@cn)
	@cid = AvocadoDB.create_collection(@cn, false)

	cmd = api + "?collection=#{@cid}"
	body = "{ \"type\" : \"geo\", \"fields\" : [ \"a\" ] }"
        #doc = AvocadoDB.post(cmd, :body => body)

	(0..10).each{|i|
	  lat = 10 * (i - 5)

	  (0..10).each{|j|
	    lon = 10 * (j - 5)
	    
	    AvocadoDB.post("/document?collection=#{@cid}", :body => "{ \"loc\" : [ #{lat}, #{lon} ] }")
	  }
	}
      end

      after do
	AvocadoDB.drop_collection(@cn)
      end

      it "returns an error for within without index" do
	cmd = api + "/within"
	body = "{ \"collection\" : \"#{@cid}\", \"latitude\" : 0, \"longitude\" : 0, \"skip\" : 1, \"limit\" : 5 }"
	doc = AvocadoDB.log_put("#{prefix}-within-missing", cmd, :body => body)

	doc.code.should eq(400)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(true)
	doc.parsed_response['code'].should eq(400)
	doc.parsed_response['errorNum'].should eq(1512)
      end

      it "returns documents within a radius" do
	cmd = "/_api/index?collection=#{@cid}"
	body = "{ \"type\" : \"geo\", \"fields\" : [ \"loc\" ] }"
        doc = AvocadoDB.post(cmd, :body => body)

	cmd = api + "/within"
	body = "{ \"collection\" : \"#{@cid}\", \"latitude\" : 0, \"longitude\" : 0, \"skip\" : 1, \"radius\" : 1111950 }"
	doc = AvocadoDB.log_put("#{prefix}-within", cmd, :body => body)

	doc.code.should eq(201)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(201)
	doc.parsed_response['hasMore'].should eq(false)
	doc.parsed_response['result'].length.should eq(4)
	doc.parsed_response['count'].should eq(4)
      end

      it "returns documents and distance within a radius" do
	cmd = "/_api/index?collection=#{@cid}"
	body = "{ \"type\" : \"geo\", \"fields\" : [ \"loc\" ] }"
        doc = AvocadoDB.post(cmd, :body => body)

	cmd = api + "/within"
	body = "{ \"collection\" : \"#{@cid}\", \"latitude\" : 0, \"longitude\" : 0, \"distance\" : \"distance\", \"radius\" : 1111950 }"
	doc = AvocadoDB.log_put("#{prefix}-within-distance", cmd, :body => body)

	doc.code.should eq(201)
	doc.headers['content-type'].should eq("application/json")
	doc.parsed_response['error'].should eq(false)
	doc.parsed_response['code'].should eq(201)
	doc.parsed_response['hasMore'].should eq(false)
	doc.parsed_response['result'].length.should eq(5)
	doc.parsed_response['count'].should eq(5)

	doc.parsed_response['result'].map{|i| i['distance'].floor.to_f}.should eq([0,1111949,1111949,1111949,1111949])
      end
    end

  end
end
