# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_api/simple"
  prefix = "api-simple"

  context "simple queries:" do

################################################################################
## geo near query
################################################################################

    context "geo query:" do
      before do
        @cn = "UnitTestsCollectionGeo"
        ArangoDB.drop_collection(@cn)
        
        body = "{ \"name\" : \"#{@cn}\", \"numberOfShards\" : 8 }"
        doc = ArangoDB.post("/_api/collection", :body => body)
        doc.code.should eq(200)
        @cid = doc.parsed_response['id']

        (0..10).each{|i|
          lat = 10 * (i - 5)

          (0..10).each{|j|
            lon = 10 * (j - 5)
            
            ArangoDB.post("/_api/document?collection=#{@cn}", :body => "{ \"loc\" : [ #{lat}, #{lon} ] }")
          }
        }
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "returns an error for near without index" do
        cmd = api + "/near"
        body = "{ \"collection\" : \"#{@cn}\", \"latitude\" : 0, \"longitude\" : 0, \"skip\" : 1, \"limit\" : 5 }"
        doc = ArangoDB.log_put("#{prefix}-near-missing", cmd, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1570)
      end

      it "returns documents near a point" do
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"geo\", \"fields\" : [ \"loc\" ] }"
        doc = ArangoDB.post(cmd, :body => body)

        cmd = api + "/near"
        body = "{ \"collection\" : \"#{@cn}\", \"latitude\" : 0, \"longitude\" : 0, \"skip\" : 1, \"limit\" : 5 }"
        doc = ArangoDB.log_put("#{prefix}-near", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(5)
        doc.parsed_response['count'].should eq(5)
      end

      it "returns documents and distance near a point" do
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"geo\", \"fields\" : [ \"loc\" ] }"
        doc = ArangoDB.post(cmd, :body => body)

        cmd = api + "/near"
        body = "{ \"collection\" : \"#{@cn}\", \"latitude\" : 0, \"longitude\" : 0, \"limit\" : 5, \"distance\" : \"distance\" }"
        doc = ArangoDB.log_put("#{prefix}-near-distance", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
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
        ArangoDB.drop_collection(@cn)

        body = "{ \"name\" : \"#{@cn}\", \"numberOfShards\" : 8 }"
        doc = ArangoDB.post("/_api/collection", :body => body)
        doc.code.should eq(200)
        @cid = doc.parsed_response['id']

        (0..10).each{|i|
          lat = 10 * (i - 5)

          (0..10).each{|j|
            lon = 10 * (j - 5)
            
            ArangoDB.post("/_api/document?collection=#{@cn}", :body => "{ \"loc\" : [ #{lat}, #{lon} ] }")
          }
        }
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "returns an error for within without index" do
        cmd = api + "/within"
        body = "{ \"collection\" : \"#{@cn}\", \"latitude\" : 0, \"longitude\" : 0, \"skip\" : 1, \"limit\" : 5 }"
        doc = ArangoDB.log_put("#{prefix}-within-missing", cmd, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1570)
      end

      it "returns documents within a radius" do
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"geo\", \"fields\" : [ \"loc\" ] }"
        doc = ArangoDB.post(cmd, :body => body)

        cmd = api + "/within"
        body = "{ \"collection\" : \"#{@cn}\", \"latitude\" : 0, \"longitude\" : 0, \"skip\" : 1, \"radius\" : 1111950 }"
        doc = ArangoDB.log_put("#{prefix}-within", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(4)
        doc.parsed_response['count'].should eq(4)
      end

      it "returns documents and distance within a radius" do
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"geo\", \"fields\" : [ \"loc\" ] }"
        doc = ArangoDB.post(cmd, :body => body)

        cmd = api + "/within"
        body = "{ \"collection\" : \"#{@cn}\", \"latitude\" : 0, \"longitude\" : 0, \"distance\" : \"distance\", \"radius\" : 1111950 }"
        doc = ArangoDB.log_put("#{prefix}-within-distance", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
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
