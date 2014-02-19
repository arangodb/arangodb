# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do
  api = "/_api/simple"
  prefix = "api-simple"

  context "simple queries:" do

################################################################################
## range query
################################################################################

    context "skiplist query:" do
      before do
        @cn = "UnitTestsCollectionSkiplist"
        ArangoDB.drop_collection(@cn)
        
        @cid = ArangoDB.create_collection(@cn, false)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end
      
      it "finds the examples, wrong index specified" do
        cmd = api + "/by-example-skiplist"
        body = "{ \"collection\" : \"#{@cn}\", \"index\" : \"1\", \"example\" : { \"i\" : 12 } }"
        doc = ArangoDB.log_put("#{prefix}-skiplist", cmd, :body => body)

        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['code'].should eq(404)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1209)
      end
      
      it "finds the examples, wrong attribute" do
        # create index
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"skiplist\", \"unique\" : true, \"fields\" : [ \"i\" ] }"
        doc = ArangoDB.log_post("#{prefix}-skiplist-index", cmd, :body => body)

        doc.code.should eq(201)
        iid = doc.parsed_response['id']

        cmd = api + "/by-example-skiplist"
        body = "{ \"collection\" : \"#{@cn}\", \"index\" : \"#{iid}\", \"example\" : { \"j\" : 12 } }"
        doc = ArangoDB.log_put("#{prefix}-skiplist", cmd, :body => body)

        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['code'].should eq(404)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1209)
      end
      
      it "finds the examples, no index specified" do
        cmd = api + "/by-example-skiplist"
        body = "{ \"collection\" : \"#{@cn}\", \"example\" : { \"i\" : 12 } }"
        doc = ArangoDB.log_put("#{prefix}-skiplist", cmd, :body => body)
   
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(0)
        doc.parsed_response['count'].should eq(0)
      end
      
      it "finds the examples, unique index" do
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
        iid = doc.parsed_response['id']
            
        # by-example-skiplist
        cmd = api + "/by-example-skiplist"
        body = "{ \"collection\" : \"#{@cn}\", \"index\" : \"#{iid}\", \"example\" : { \"i\" : 3 } }"
        doc = ArangoDB.log_put("#{prefix}-skiplist", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(1)
        doc.parsed_response['count'].should eq(1)
        doc.parsed_response['result'].map{|i| i['i']}.should =~ [3]
        
        body = "{ \"collection\" : \"#{@cn}\", \"index\" : \"#{iid}\", \"example\" : { \"i\" : 12 } }"
        doc = ArangoDB.log_put("#{prefix}-skiplist", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(0)
        doc.parsed_response['count'].should eq(0)
      end
       
      it "finds the examples, non-unique index" do
        # create data
        for i in [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ]
          body = "{ \"i\" : #{i}, \"j\" : 2 }"
          doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
          doc.code.should eq(202)
        end

        # create index
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"skiplist\", \"unique\" : false, \"fields\" : [ \"j\", \"i\" ] }"
        doc = ArangoDB.log_post("#{prefix}-skiplist-index", cmd, :body => body)

        doc.code.should eq(201)
        iid = doc.parsed_response['id']
            
        # by-example-skiplist
        cmd = api + "/by-example-skiplist"
        body = "{ \"collection\" : \"#{@cn}\", \"index\" : \"#{iid}\", \"example\" : { \"j\" : 2, \"i\" : 2 } }"
        doc = ArangoDB.log_put("#{prefix}-skiplist", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(1)
        doc.parsed_response['count'].should eq(1)
        doc.parsed_response['result'].map{|i| i['i']}.should =~ [2]
        doc.parsed_response['result'].map{|i| i['j']}.should =~ [2]
        
        body = "{ \"collection\" : \"#{@cn}\", \"index\" : \"#{iid}\", \"example\" : { \"i\" : 2, \"j\" : 2 } }"
        doc = ArangoDB.log_put("#{prefix}-skiplist", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(1)
        doc.parsed_response['count'].should eq(1)
        doc.parsed_response['result'].map{|i| i['i']}.should =~ [2]
        doc.parsed_response['result'].map{|i| i['j']}.should =~ [2]
        
        body = "{ \"collection\" : \"#{@cn}\", \"index\" : \"#{iid}\", \"example\" : { \"j\" : 3, \"i\" : 2 } }"
        doc = ArangoDB.log_put("#{prefix}-skiplist", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(0)
        doc.parsed_response['count'].should eq(0)
      end
      
      it "finds the examples" do
        # create data
  
        (0..10).each do |i|
          (0..10).each do |j|
            body = "{ \"i\" : #{i}, \"j\" : #{j} }"
            doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
            doc.code.should eq(202)
          end
        end
        
        # create index
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"skiplist\", \"unique\" : false, \"fields\" : [ \"i\", \"j\" ] }"
        doc = ArangoDB.log_post("#{prefix}-skiplist-index", cmd, :body => body)

        doc.code.should eq(201)
        iid = doc.parsed_response['id']
            
        # by-example-skiplist
        cmd = api + "/by-example-skiplist"
        body = "{ \"collection\" : \"#{@cn}\", \"index\" : \"#{iid}\", \"example\" : { \"i\" : 1, \"j\" : 7 } }"
        doc = ArangoDB.log_put("#{prefix}-skiplist", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(1)
        doc.parsed_response['count'].should eq(1)
        doc.parsed_response['result'].map{|i| i['i']}.should =~ [1]
        doc.parsed_response['result'].map{|i| i['j']}.should =~ [7]
      end
      
      it "finds the examples, multiple elements" do
        # create data
  
        (0..9).each do |i|
          (0..9).each do |j|
            body = "{ \"i\" : #{i}, \"j\" : #{j} }"
            doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
            doc.code.should eq(202)
          end
        end
        
        # create index
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"skiplist\", \"unique\" : false, \"fields\" : [ \"i\" ] }"
        doc = ArangoDB.log_post("#{prefix}-skiplist-index", cmd, :body => body)

        doc.code.should eq(201)
        iid = doc.parsed_response['id']
            
        # by-example-skiplist
        cmd = api + "/by-example-skiplist"
        body = "{ \"collection\" : \"#{@cn}\", \"index\" : \"#{iid}\", \"example\" : { \"i\" : 1 } }"
        doc = ArangoDB.log_put("#{prefix}-skiplist", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(10)
        doc.parsed_response['count'].should eq(10)
        doc.parsed_response['result'].map{|i| i['i']}.should =~ [1,1,1,1,1,1,1,1,1,1]
      end
      
      it "finds the examples, small batchsize" do
        # create data
  
        (0..9).each do |i|
          (0..9).each do |j|
            body = "{ \"i\" : #{i}, \"j\" : #{j} }"
            doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
            doc.code.should eq(202)
          end
        end
        
        # create index
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"skiplist\", \"unique\" : false, \"fields\" : [ \"i\" ] }"
        doc = ArangoDB.log_post("#{prefix}-skiplist-index", cmd, :body => body)

        doc.code.should eq(201)
        iid = doc.parsed_response['id']
            
        # by-example-skiplist
        cmd = api + "/by-example-skiplist"
        body = "{ \"collection\" : \"#{@cn}\", \"index\" : \"#{iid}\", \"example\" : { \"i\" : 0 }, \"batchSize\" : 2 }"
        doc = ArangoDB.log_put("#{prefix}-skiplist", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['result'].length.should eq(2)
        doc.parsed_response['count'].should eq(10)
        doc.parsed_response['result'].map{|i| i['i']}.should =~ [0,0]
      end
    end

  end
end
