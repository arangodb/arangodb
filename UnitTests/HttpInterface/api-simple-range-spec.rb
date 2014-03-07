# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_api/simple"
  prefix = "api-simple"

  context "simple queries:" do

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
        doc.parsed_response['result'].map{|i| i['i']}.should eq([2,3])

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
        doc.parsed_response['result'].map{|i| i['i']}.should eq([2,3,4])
      end
      
      it "finds the examples, big result" do
        # create data
        (0..499).each do |i|
          body = "{ \"i\" : #{i} }"
          doc = ArangoDB.post("/_api/document?collection=#{@cn}", :body => body)
          doc.code.should eq(202)
        end

        # create index
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"skiplist\", \"unique\" : false, \"fields\" : [ \"i\" ] }"
        doc = ArangoDB.log_post("#{prefix}-range", cmd, :body => body)

        doc.code.should eq(201)
            
        # range
        cmd = api + "/range"
        body = "{ \"collection\" : \"#{@cn}\", \"attribute\" : \"i\", \"left\" : 5, \"right\" : 498 }"
        doc = ArangoDB.log_put("#{prefix}-range", cmd, :body => body)
        
        cmp = [ ]
        (5..497).each do |i|
          cmp.push i
        end

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(493)
        doc.parsed_response['count'].should eq(493)
        doc.parsed_response['result'].map{|i| i['i']}.should eq(cmp)

        # closed range
        body = "{ \"collection\" : \"#{@cn}\", \"attribute\" : \"i\", \"left\" : 2, \"right\" : 498, \"closed\" : true }"
        doc = ArangoDB.log_put("#{prefix}-range", cmd, :body => body)
        
        cmp = [ ]
        (2..498).each do |i|
          cmp.push i
        end

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(497)
        doc.parsed_response['count'].should eq(497)
        doc.parsed_response['result'].map{|i| i['i']}.should eq(cmp)
      end
    end

  end
end
