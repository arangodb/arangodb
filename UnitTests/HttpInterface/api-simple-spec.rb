# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do
  api = "/_api/simple"
  prefix = "api-simple"

  context "simple queries:" do

################################################################################
## all query
################################################################################

    context "all query (will take a while when using ssl):" do
      before do
        @cn = "UnitTestsCollectionSimple"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn, false)


        ArangoDB.post("/_admin/execute", :body => "var db = require('internal').db, c = db.#{@cn}; for (var i = 0; i < 1500; ++i) { c.save({ n: i }); }")
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "get all documents" do
        cmd = api + "/all"
        body = "{ \"collection\" : \"#{@cn}\" }"
        doc = ArangoDB.log_put("#{prefix}-all", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['result'].length.should eq(1000)
        doc.parsed_response['count'].should eq(1500)
      end

      it "get all documents with limit" do
        cmd = api + "/all"
        body = "{ \"collection\" : \"#{@cn}\", \"limit\" : 100 }"
        doc = ArangoDB.log_put("#{prefix}-all-limit", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(100)
        doc.parsed_response['count'].should eq(100)
      end

      it "get all documents with negative skip" do
        cmd = api + "/all"
        body = "{ \"collection\" : \"#{@cn}\", \"skip\" : -100 }"
        doc = ArangoDB.log_put("#{prefix}-all-negative-limit", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(100)
        doc.parsed_response['count'].should eq(100)
      end

      it "get all documents with skip" do
        cmd = api + "/all"
        body = "{ \"collection\" : \"#{@cn}\", \"skip\" : 1400 }"
        doc = ArangoDB.log_put("#{prefix}-all-skip", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(100)
        doc.parsed_response['count'].should eq(100)
      end

      it "get all documents with skip and limit" do
        cmd = api + "/all"
        body = "{ \"collection\" : \"#{@cn}\", \"skip\" : 1400, \"limit\" : 2 }"
        doc = ArangoDB.log_put("#{prefix}-all-skip-limit", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(2)
        doc.parsed_response['count'].should eq(2)
      end
    end

################################################################################
## any query
################################################################################

    context "any query:" do
      before do
        @cn = "UnitTestsCollectionSimple"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn, false)

        (0...10).each{|i|
          ArangoDB.post("/_api/document?collection=#{@cid}", :body => "{ \"n\" : #{i} }")
        }
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "get any documents" do
        cmd = api + "/any"
        body = "{ \"collection\" : \"#{@cid}\" }"
        doc = ArangoDB.log_put("#{prefix}-any", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['document']['n'].should be_kind_of(Integer)
      end
    end

################################################################################
## geo near query
################################################################################

    context "geo query:" do
      before do
        @cn = "UnitTestsCollectionGeo"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn, false)

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
        @cid = ArangoDB.create_collection(@cn, false)

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

################################################################################
## first / last
################################################################################

    context "first and last queries:" do
      before do
        @cn = "UnitTestsCollectionByExample"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn, false)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "finds the examples" do
        body = "{ \"i\" : 1 }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)
        d1 = doc.parsed_response['_id']

        body = "{ \"i\" : 1, \"a\" : { \"j\" : 1 } }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)
        d2 = doc.parsed_response['_id']

        body = "{ \"i\" : 1, \"a\" : { \"j\" : 1, \"k\" : 1 } }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)
        d3 = doc.parsed_response['_id']

        body = "{ \"i\" : 1, \"a\" : { \"j\" : 2, \"k\" : 2 } }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)
        d4 = doc.parsed_response['_id']

        body = "{ \"i\" : 2 }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)
        d5 = doc.parsed_response['_id']

        body = "{ \"i\" : 2, \"a\" : 2 }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)
        d6 = doc.parsed_response['_id']

        body = "{ \"i\" : 2, \"a\" : { \"j\" : 2, \"k\" : 2 } }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)
        d7 = doc.parsed_response['_id']

        
        cmd = api + "/first"
        body = "{ \"collection\" : \"#{@cn}\" }"
        doc = ArangoDB.log_put("#{prefix}-first1", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['result']['_id'].should eq(d1)
        
        cmd = api + "/first"
        body = "{ \"collection\" : \"#{@cn}\", \"count\" : 2 }"
        doc = ArangoDB.log_put("#{prefix}-first2", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['result'].length.should eq(2)
        doc.parsed_response['result'].map{|i| i['_id']}.should =~ [d1,d2]

        cmd = api + "/last"
        body = "{ \"collection\" : \"#{@cn}\" }"
        doc = ArangoDB.log_put("#{prefix}-last1", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['result']['_id'].should eq(d7)
        
        cmd = api + "/last"
        body = "{ \"collection\" : \"#{@cn}\", \"count\" : 2 }"
        doc = ArangoDB.log_put("#{prefix}-last2", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['result'].length.should eq(2)
        doc.parsed_response['result'].map{|i| i['_id']}.should =~ [d7,d6]
      end
    end

################################################################################
## by-example query
################################################################################

    context "by-example query:" do
      before do
        @cn = "UnitTestsCollectionByExample"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn, false)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "finds the examples" do
        body = "{ \"i\" : 1 }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)
        d1 = doc.parsed_response['_id']

        body = "{ \"i\" : 1, \"a\" : { \"j\" : 1 } }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)
        d2 = doc.parsed_response['_id']

        body = "{ \"i\" : 1, \"a\" : { \"j\" : 1, \"k\" : 1 } }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)
        d3 = doc.parsed_response['_id']

        body = "{ \"i\" : 1, \"a\" : { \"j\" : 2, \"k\" : 2 } }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)
        d4 = doc.parsed_response['_id']

        body = "{ \"i\" : 2 }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)
        d5 = doc.parsed_response['_id']

        body = "{ \"i\" : 2, \"a\" : 2 }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)
        d6 = doc.parsed_response['_id']

        body = "{ \"i\" : 2, \"a\" : { \"j\" : 2, \"k\" : 2 } }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)
        d7 = doc.parsed_response['_id']

        cmd = api + "/by-example"
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"i\" : 1 } }"
        doc = ArangoDB.log_put("#{prefix}-by-example1", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(4)
        doc.parsed_response['count'].should eq(4)
        doc.parsed_response['result'].map{|i| i['_id']}.should =~ [d1,d2,d3,d4]

        cmd = api + "/by-example"
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"a\" : { \"j\" : 1 } } }"
        doc = ArangoDB.log_put("#{prefix}-by-example2", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(1)
        doc.parsed_response['count'].should eq(1)
        doc.parsed_response['result'].map{|i| i['_id']}.should =~ [d2]

        cmd = api + "/by-example"
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"a.j\" : 1 } }"
        doc = ArangoDB.log_put("#{prefix}-by-example3", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(2)
        doc.parsed_response['count'].should eq(2)
        doc.parsed_response['result'].map{|i| i['_id']}.should =~ [d2,d3]

        cmd = api + "/first-example"
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"a.j\" : 1, \"a.k\" : 1 } }"
        doc = ArangoDB.log_put("#{prefix}-first-example", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['document']['_id'].should eq(d3)

        cmd = api + "/first-example"
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"a.j\" : 1, \"a.k\" : 2 } }"
        doc = ArangoDB.log_put("#{prefix}-first-example-not-found", cmd, :body => body)

        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
      end
    end

################################################################################
## by-example query with skip / limit
################################################################################

    context "by-example query with skip:" do
      before do
        @cn = "UnitTestsCollectionByExample"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn, false)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "finds the examples" do
        body = "{ \"someAttribute\" : \"someValue\", \"someOtherAttribute\" : \"someOtherValue\" }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)

        body = "{ \"someAttribute\" : \"someValue\", \"someOtherAttribute2\" : \"someOtherValue2\" }"
        doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
        doc.code.should eq(202)

        cmd = api + "/by-example"
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"someAttribute\" : \"someValue\" } }"
        doc = ArangoDB.log_put("#{prefix}-by-example-skip", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(2)
        doc.parsed_response['count'].should eq(2)

        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"someAttribute\" : \"someValue\" }, \"skip\" : 1 }"
        doc = ArangoDB.log_put("#{prefix}-by-example-skip", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(1)
        doc.parsed_response['count'].should eq(1)
        
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"someAttribute\" : \"someValue\" }, \"skip\" : 1, \"limit\" : 1 }"
        doc = ArangoDB.log_put("#{prefix}-by-example-skip", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(1)
        doc.parsed_response['count'].should eq(1)
        
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"someAttribute\" : \"someValue\" }, \"skip\" : 2 }"
        doc = ArangoDB.log_put("#{prefix}-by-example-skip", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(0)
        doc.parsed_response['count'].should eq(0)
      end
    end

################################################################################
## remove-by-example query
################################################################################

    context "remove-by-example query:" do
      before do
        @cn = "UnitTestsCollectionByExample"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn, false)
  
        (0...20).each{|i|
          ArangoDB.post("/_api/document?collection=#{@cn}", :body => "{ \"value\" : #{i}, \"value2\" : 99 }")
        }
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "removes the examples" do
        cmd = api + "/remove-by-example"
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"value\" : 1 } }"
        doc = ArangoDB.log_put("#{prefix}-remove-by-example", cmd, :body => body)

        # remove first
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['deleted'].should eq(1)
        
        # remove again
        doc = ArangoDB.log_put("#{prefix}-remove-by-example", cmd, :body => body)
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['deleted'].should eq(0)


        # remove other doc
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"value\" : 2 } }"
        doc = ArangoDB.log_put("#{prefix}-remove-by-example", cmd, :body => body)

        # remove first
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['deleted'].should eq(1)
        
        # remove again
        doc = ArangoDB.log_put("#{prefix}-remove-by-example", cmd, :body => body)
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['deleted'].should eq(0)
       
        
        # remove others
        (3...8).each{|i|
          body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"value\" : #{i} } }"
          doc = ArangoDB.log_put("#{prefix}-remove-by-example", cmd, :body => body)

          doc.code.should eq(200)
          doc.headers['content-type'].should eq("application/json; charset=utf-8")
          doc.parsed_response['error'].should eq(false)
          doc.parsed_response['code'].should eq(200)
          doc.parsed_response['deleted'].should eq(1)
        
          doc = ArangoDB.log_put("#{prefix}-remove-by-example", cmd, :body => body)
          doc.parsed_response['deleted'].should eq(0)
        }

        # remove non-existing values
        [ 21, 22, 100, 101, 99, "\"meow\"", "\"\"", "\"null\"" ].each{|value|
          body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"value\" : " + value.to_s + " } }"
          doc = ArangoDB.log_put("#{prefix}-remove-by-example", cmd, :body => body)
          doc.code.should eq(200)
          doc.headers['content-type'].should eq("application/json; charset=utf-8")
          doc.parsed_response['error'].should eq(false)
          doc.parsed_response['code'].should eq(200)
          doc.parsed_response['deleted'].should eq(0)
        }

        # remove non-existing attributes
        [ "value2", "value3", "fox", "meow" ].each{|value|
          body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"" + value + "\" : 1 } }"
          doc = ArangoDB.log_put("#{prefix}-remove-by-example", cmd, :body => body)

          doc.code.should eq(200)
          doc.headers['content-type'].should eq("application/json; charset=utf-8")
          doc.parsed_response['error'].should eq(false)
          doc.parsed_response['code'].should eq(200)
          doc.parsed_response['deleted'].should eq(0)
        }
        
        # insert 10 identical documents
        (0...10).each{|i|
          ArangoDB.post("/_api/document?collection=#{@cn}", :body => "{ \"value99\" : 7, \"value98\" : 1 }")
        }

        # miss them
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"value99\" : 7, \"value98\" : 2} }"
        doc = ArangoDB.log_put("#{prefix}-remove-by-example", cmd, :body => body)
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['deleted'].should eq(0)
              
        # miss them again
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"value99\" : 70, \"value98\" : 1} }"
        doc = ArangoDB.log_put("#{prefix}-remove-by-example", cmd, :body => body)
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['deleted'].should eq(0)
              
        # now remove them
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"value99\" : 7, \"value98\" : 1} }"
        doc = ArangoDB.log_put("#{prefix}-remove-by-example", cmd, :body => body)
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['deleted'].should eq(10)

        # remove again
        doc = ArangoDB.log_put("#{prefix}-remove-by-example", cmd, :body => body)
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['deleted'].should eq(0)
      end
      
      it "removes the examples, with limit" do
        cmd = api + "/remove-by-example"
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"value2\" : 99 }, \"limit\" : 5 }"
        doc = ArangoDB.log_put("#{prefix}-remove-by-example-limit", cmd, :body => body)

        # remove some
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['deleted'].should eq(5)
        
        # remove some more
        doc = ArangoDB.log_put("#{prefix}-remove-by-example-limit", cmd, :body => body)
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['deleted'].should eq(5)

        # remove the rest
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"value2\" : 99 }, \"limit\" : 50 }"
        doc = ArangoDB.log_put("#{prefix}-remove-by-example-limit", cmd, :body => body)
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['deleted'].should eq(10)
      end
    end

################################################################################
## replace-by-example query
################################################################################

    context "replace-by-example query:" do
      before do
        @cn = "UnitTestsCollectionByExample"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn, false)
  
        (0...20).each{|i|
          ArangoDB.post("/_api/document?collection=#{@cn}", :body => "{ \"value\" : #{i}, \"value2\" : 99 }")
        }
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "replaces the examples" do
        cmd = api + "/replace-by-example"
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"value\" : 1 }, \"newValue\" : { \"foo\" : \"bar\" } }"
        doc = ArangoDB.log_put("#{prefix}-replace-by-example", cmd, :body => body)

        # replace one
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['replaced'].should eq(1)
        
        # replace other
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"value\" : 2 }, \"newValue\" : { \"foo\" : \"baz\" } }"
        doc = ArangoDB.log_put("#{prefix}-replace-by-example", cmd, :body => body)
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['replaced'].should eq(1)
        
        # replace all others
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"value2\" : 99 }, \"newValue\" : { \"moo\" : \"fox\" } }"
        doc = ArangoDB.log_put("#{prefix}-replace-by-example", cmd, :body => body)
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['replaced'].should eq(18)
        
        # remove non-existing values
        [ 21, 22, 100, 101, 99, "\"meow\"", "\"\"", "\"null\"" ].each{|value|
          body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"value\" : " + value.to_s + " }, \"newValue\" : { } }"
          doc = ArangoDB.log_put("#{prefix}-replace-by-example", cmd, :body => body)
          doc.code.should eq(200)
          doc.headers['content-type'].should eq("application/json; charset=utf-8")
          doc.parsed_response['error'].should eq(false)
          doc.parsed_response['code'].should eq(200)
          doc.parsed_response['replaced'].should eq(0)
        }
      end
      
      it "replaces the examples, with limit" do
        cmd = api + "/replace-by-example"
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"value2\" : 99 }, \"newValue\" : { \"foo\" : \"bar\" }, \"limit\" : 5 }"
        doc = ArangoDB.log_put("#{prefix}-replace-by-example-limit", cmd, :body => body)

        # replace some
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['replaced'].should eq(5)
        
        # replace some more
        doc = ArangoDB.log_put("#{prefix}-replace-by-example-limit", cmd, :body => body)
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['replaced'].should eq(5)

        # replace the rest
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"value2\" : 99 }, \"newValue\" : { \"fox\" : \"box\" }, \"limit\" : 50 }"
        doc = ArangoDB.log_put("#{prefix}-replace-by-example-limit", cmd, :body => body)
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['replaced'].should eq(10)
      end
    end

################################################################################
## update-by-example query
################################################################################

    context "update-by-example query:" do
      before do
        @cn = "UnitTestsCollectionByExample"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn, false)
  
        (0...20).each{|i|
          ArangoDB.post("/_api/document?collection=#{@cn}", :body => "{ \"value\" : #{i}, \"value2\" : 99 }")
        }
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "updates the examples" do
        cmd = api + "/update-by-example"
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"value\" : 1 }, \"newValue\" : { \"foo\" : \"bar\" } }"
        doc = ArangoDB.log_put("#{prefix}-update-by-example", cmd, :body => body)

        # update one
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['updated'].should eq(1)
        
        # update other
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"value\" : 2 }, \"newValue\" : { \"foo\" : \"baz\" } }"
        doc = ArangoDB.log_put("#{prefix}-update-by-example", cmd, :body => body)
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['updated'].should eq(1)
        
        # update other, overwrite
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"value\" : 3 }, \"newValue\" : { \"foo\" : \"baz\", \"value\" : 12 } }"
        doc = ArangoDB.log_put("#{prefix}-update-by-example", cmd, :body => body)
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['updated'].should eq(1)
        
        # update other, remove
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"value\" : 12 }, \"newValue\" : { \"value2\" : null }, \"keepNull\" : false }"
        doc = ArangoDB.log_put("#{prefix}-update-by-example", cmd, :body => body)
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['updated'].should eq(2)
        
        # update all but the 2 from before
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"value2\" : 99 }, \"newValue\" : { \"moo\" : \"fox\" } }"
        doc = ArangoDB.log_put("#{prefix}-update-by-example", cmd, :body => body)
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['updated'].should eq(18)
        
        # update non-existing values
        [ 100, 101, 99, "\"meow\"", "\"\"", "\"null\"" ].each{|value|
          body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"value\" : " + value.to_s + " }, \"newValue\" : { } }"
          doc = ArangoDB.log_put("#{prefix}-update-by-example", cmd, :body => body)
          doc.code.should eq(200)
          doc.headers['content-type'].should eq("application/json; charset=utf-8")
          doc.parsed_response['error'].should eq(false)
          doc.parsed_response['code'].should eq(200)
          doc.parsed_response['updated'].should eq(0)
        }
      end
      
      it "updates the examples, with limit" do
        cmd = api + "/update-by-example"
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"value2\" : 99 }, \"newValue\" : { \"foo\" : \"bar\", \"value2\" : 17 }, \"limit\" : 5 }"
        doc = ArangoDB.log_put("#{prefix}-update-by-example-limit", cmd, :body => body)

        # update some
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['updated'].should eq(5)
        
        # update some more
        doc = ArangoDB.log_put("#{prefix}-update-by-example-limit", cmd, :body => body)
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['updated'].should eq(5)

        # update the rest
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"value2\" : 99 }, \"newValue\" : { \"fox\" : \"box\" }, \"limit\" : 50 }"
        doc = ArangoDB.log_put("#{prefix}-replace-by-example-limit", cmd, :body => body)
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['updated'].should eq(10)
      end
    end

################################################################################
## range query
################################################################################

    context "range query:" do
      before do
        @cn = "UnitTestsCollectionRange"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn, false)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "finds the examples" do
        # create data
        for i in [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ]
          body = "{ \"i\" : #{i} }"
          doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
          doc.code.should eq(202)
        end

        # create index
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"skiplist\", \"unique\" : true, \"fields\" : [ \"i\" ] }"
        doc = ArangoDB.log_post("#{prefix}-skiplist-index", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['type'].should eq("skiplist")
        doc.parsed_response['unique'].should eq(true)
            
        # range
        cmd = api + "/range"
        body = "{ \"collection\" : \"#{@cn}\", \"attribute\" : \"i\", \"left\" : 2, \"right\" : 4 }"
        doc = ArangoDB.log_put("#{prefix}-range", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(2)
        doc.parsed_response['count'].should eq(2)
        doc.parsed_response['result'].map{|i| i['i']}.should =~ [2,3]

        # closed range
        cmd = api + "/range"
        body = "{ \"collection\" : \"#{@cn}\", \"attribute\" : \"i\", \"left\" : 2, \"right\" : 4, \"closed\" : true }"
        doc = ArangoDB.log_put("#{prefix}-range", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(3)
        doc.parsed_response['count'].should eq(3)
        doc.parsed_response['result'].map{|i| i['i']}.should =~ [2,3,4]
      end
    end

################################################################################
## fulltext query
################################################################################

    context "fulltext query:" do
      before do
        @cn = "UnitTestsCollectionFulltext"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn, false)

        texts = [
          "Nuevo. Ella ni caso a esa señora pagó leer Invitación amistad viajando comer todo lo que el a dos. Shy ustedes que apenas gastos debatiendo apresuró resuelto. Siempre educado momento en que es espíritu calor a los corazones. Downs esos ingeniosos aún un jefe bolas tan así. Momento un poco hasta quedarse sin ninguna animado. Camino de mayo trajo a nuestro país periódicas para adaptarse vitorearon.",
          "Él había enviado invitación bullicioso conexión habitar proyección. Por mutuo un peligro desván mr edward una. Desviado como adición esfuerzo estrictamente ninguna disposición por Stanhill. Esta mujer llamada lo hacen suspirar puerta no sentía. Usted y el orden morada pesar conseguirlo. La adquisición de lejos nuestra pertenencia a nosotros mismos y ciertamente propio perpetuo continuo. Es otra parte de mi a veces o certeza. Lain no como cinco o menos alto. Todo viajar establecer cómo la literatura ley.",
          "Se trató de chistes en bolsa china decaimiento listo un archivo. Pequeño su timidez tenía leñosa poder downs. Para que denota habla admitió aprendiendo mi ejercicio para Adquiridos pulg persianas mr lo sentimientos. Para o tres casa oferta tomado am a comenzar. Como disuadir alegre superó así de amable se entregaba sin envasar. Alteración de conexión así como me coleccionismo. Difícil entregado en extenso en subsidio dirección. Alteración poner disminución uso puede considerarse sentimientos discreción interesado. Un viendo débilmente escaleras soy yo sin ingresos rama.",
          "Contento obtener certeza desconfía más aún son jamón franqueza oculta. En la resolución no afecta a considerar de. No me pareció marido o coronel efectos formando. Shewing End sentado que vio además de musical adaptado hijo. En contraste interesados comer pianoforte alteración simpatizar fue. Él cree que si las familias no sorprender a un interés elegancia. Reposó millas equivocadas una placa tan demora. Ella puso propia relación sobrevivió podrá eliminarse."
        ]

        # insert documents
        texts.each do |text|
          ArangoDB.post("/_api/document?collection=#{@cn}", :body => "{ \"text\" : \"#{text}\" }")
        end
      end

      after do
        ArangoDB.drop_collection(@cn)
      end
      
      it "returns an error for fulltext query without query attribute" do
        cmd = api + "/fulltext"
        body = "{ \"collection\" : \"#{@cn}\", \"attribute\" : \"text\" }"
        doc = ArangoDB.log_put("#{prefix}-fulltext-missing", cmd, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
      end

      it "returns an error for fulltext query without an index" do
        cmd = api + "/fulltext"
        body = "{ \"collection\" : \"#{@cn}\", \"attribute\" : \"text\", \"query\" : \"foo\" }"
        doc = ArangoDB.log_put("#{prefix}-fulltext-missing", cmd, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1571)
      end
      
      it "returns an error for fulltext query with a wrong index" do
        # create index
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"fulltext\", \"fields\" : [ \"text\" ] }"
        doc = ArangoDB.post(cmd, :body => body)

              # query index on different attribute
        cmd = api + "/fulltext"
        body = "{ \"collection\" : \"#{@cn}\", \"attribute\" : \"foo\", \"query\" : \"bar\" }"
        doc = ArangoDB.log_put("#{prefix}-fulltext-missing", cmd, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1571)
      end

      it "returns documents for fulltext queries" do
        # create index
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"fulltext\", \"fields\" : [ \"text\" ] }"
        doc = ArangoDB.post(cmd, :body => body)

        cmd = api + "/fulltext"
        body = "{ \"collection\" : \"#{@cn}\", \"attribute\" : \"text\", \"query\" : \"como,interesado,viendo\" }"
        doc = ArangoDB.log_put("#{prefix}-within", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(1)
        doc.parsed_response['count'].should eq(1)
      end
      
      it "returns documents for fulltext queries" do
        # create index
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"fulltext\", \"fields\" : [ \"text\" ] }"
        doc = ArangoDB.post(cmd, :body => body)

        cmd = api + "/fulltext"
        body = "{ \"collection\" : \"#{@cn}\", \"attribute\" : \"text\", \"query\" : \"que\", \"limit\" : 2 }"
        doc = ArangoDB.log_put("#{prefix}-within", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(2)
        doc.parsed_response['count'].should eq(2)
      end
      
      it "returns documents for fulltext queries" do
        # create index
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"fulltext\", \"fields\" : [ \"text\" ] }"
        doc = ArangoDB.post(cmd, :body => body)

        cmd = api + "/fulltext"
        body = "{ \"collection\" : \"#{@cn}\", \"attribute\" : \"text\", \"query\" : \"prefix:que,prefix:pa\" }"
        doc = ArangoDB.log_put("#{prefix}-within", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(3)
        doc.parsed_response['count'].should eq(3)
      end
    end

  end
end
