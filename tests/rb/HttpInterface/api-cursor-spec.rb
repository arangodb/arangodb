# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_api/cursor"
  prefix = "api-cursor"

  context "dealing with cursors:" do
    before do
      @reId = Regexp.new('^\d+$')
    end

################################################################################
## error handling
################################################################################

    context "error handling:" do
      it "returns an error if body is missing" do
        cmd = api
        doc = ArangoDB.log_post("#{prefix}-missing-body", cmd)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1502)
      end
      
      it "returns an error if query attribute is missing" do
        cmd = api
        body = "{ }"
        doc = ArangoDB.log_post("#{prefix}-empty-query", cmd, :body => body)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(600)
      end
      
      it "returns an error if query is null" do
        cmd = api
        body = "{ \"query\" : null }"
        doc = ArangoDB.log_post("#{prefix}-empty-query", cmd, :body => body)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1502)
      end
      
      it "returns an error if query string is empty" do
        cmd = api
        body = "{ \"query\" : \"\" }"
        doc = ArangoDB.log_post("#{prefix}-empty-query", cmd, :body => body)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1502)
      end
      
      it "returns an error if query string is just whitespace" do
        cmd = api
        body = "{ \"query\" : \"    \" }"
        doc = ArangoDB.log_post("#{prefix}-empty-query", cmd, :body => body)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1501)
      end

      it "returns an error if collection is unknown" do
        cmd = api
        body = "{ \"query\" : \"FOR u IN unknowncollection LIMIT 2 RETURN u.n\", \"count\" : true, \"bindVars\" : {}, \"batchSize\" : 2 }"
        doc = ArangoDB.log_post("#{prefix}-unknown-collection", cmd, :body => body)
        
        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
        doc.parsed_response['errorNum'].should eq(1203)
      end

      it "returns an error if cursor identifier is missing" do
        cmd = api
        doc = ArangoDB.log_put("#{prefix}-missing-cursor-identifier", cmd)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(400)
      end

      it "returns an error if cursor identifier is invalid" do
        cmd = api + "/123456"
        doc = ArangoDB.log_put("#{prefix}-invalid-cursor-identifier", cmd)
        
        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
        doc.parsed_response['errorNum'].should eq(1600)
      end
      
      it "returns an error if memory limit is violated" do
        cmd = api
        body = "{ \"query\" : \"FOR i IN 1..100000 SORT i RETURN i\", \"memoryLimit\" : 100000 }"
        doc = ArangoDB.log_post("#{prefix}-memory-limit", cmd, :body => body)
        
        doc.code.should eq(500)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(500)
        doc.parsed_response['errorNum'].should eq(32)
      end
      
      it "returns no errors but warnings if fail-on-warning is not triggered" do
        cmd = api
        body = "{ \"query\" : \"FOR i IN 1..5 RETURN i / 0\", \"options\" : { \"failOnWarning\" : false } }"
        doc = ArangoDB.log_post("#{prefix}-fail-on-warning", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(5)
        doc.parsed_response['result'].should eq([ nil, nil, nil, nil, nil ])
        doc.parsed_response['extra']['warnings'].length.should eq(5)
        doc.parsed_response['extra']['warnings'][0]['code'].should eq(1562)
        doc.parsed_response['extra']['warnings'][1]['code'].should eq(1562)
        doc.parsed_response['extra']['warnings'][2]['code'].should eq(1562)
        doc.parsed_response['extra']['warnings'][3]['code'].should eq(1562)
        doc.parsed_response['extra']['warnings'][4]['code'].should eq(1562)
      end
      
      it "returns no errors but warnings if fail-on-warning is not triggered, limiting number of warnings" do
        cmd = api
        body = "{ \"query\" : \"FOR i IN 1..5 RETURN i / 0\", \"options\" : { \"failOnWarning\" : false, \"maxWarningCount\" : 3 } }"
        doc = ArangoDB.log_post("#{prefix}-fail-on-warning", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(5)
        doc.parsed_response['result'].should eq([ nil, nil, nil, nil, nil ])
        doc.parsed_response['extra']['warnings'].length.should eq(3)
        doc.parsed_response['extra']['warnings'][0]['code'].should eq(1562)
        doc.parsed_response['extra']['warnings'][1]['code'].should eq(1562)
        doc.parsed_response['extra']['warnings'][2]['code'].should eq(1562)
      end
      
      it "returns an error if fail-on-warning is triggered" do
        cmd = api
        body = "{ \"query\" : \"FOR i IN 1..5 RETURN i / 0\", \"options\" : { \"failOnWarning\" : true } }"
        doc = ArangoDB.log_post("#{prefix}-fail-on-warning", cmd, :body => body)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1562)
      end

    end

################################################################################
## create and using cursors, continuation
################################################################################

    context "handling a cursor with continuation:" do
      before do
        @cn = "users"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn, false)

        (0...2001).each{|i|
          ArangoDB.post("/_api/document?collection=#{@cid}", :body => "{ \"_key\" : \"test#{i}\" }")
        }
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "creates a cursor" do
        cmd = api
        body = "{ \"query\" : \"FOR u IN #{@cn} RETURN u\", \"count\" : true }"
        doc = ArangoDB.log_post("#{prefix}-create-batchsize", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(2001)
        doc.parsed_response['result'].length.should eq(1000)
        doc.parsed_response['cached'].should eq(false)

        id = doc.parsed_response['id']

        cmd = api + "/#{id}"
        doc = ArangoDB.log_put("#{prefix}-create-batchsize", cmd)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['id'].should eq(id)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(2001)
        doc.parsed_response['result'].length.should eq(1000)
        doc.parsed_response['cached'].should eq(false)

        cmd = api + "/#{id}"
        doc = ArangoDB.log_put("#{prefix}-create-batchsize", cmd)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['count'].should eq(2001)
        doc.parsed_response['result'].length.should eq(1)
        doc.parsed_response['cached'].should eq(false)

        cmd = api + "/#{id}"
        doc = ArangoDB.log_put("#{prefix}-create-batchsize", cmd)
        
        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1600)
        doc.parsed_response['code'].should eq(404)
      end
    end

################################################################################
## create and using cursors
################################################################################

    context "handling a cursor:" do
      before do
        @cn = "users"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn, false)

        (0...10).each{|i|
          ArangoDB.post("/_api/document?collection=#{@cid}", :body => "{ \"n\" : #{i} }")
        }
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "creates a cursor single run" do
        cmd = api
        body = "{ \"query\" : \"FOR u IN #{@cn} LIMIT 2 RETURN u.n\", \"count\" : true, \"bindVars\" : {}, \"batchSize\" : 2 }"
        doc = ArangoDB.log_post("#{prefix}-create-for-limit-return-single", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['count'].should eq(2)
        doc.parsed_response['result'].length.should eq(2)
        doc.parsed_response['cached'].should eq(false)
      end
      
      it "creates a cursor single run, without count" do
        cmd = api
        body = "{ \"query\" : \"FOR u IN #{@cn} LIMIT 2 RETURN u.n\", \"count\" : false, \"bindVars\" : {} }"
        doc = ArangoDB.log_post("#{prefix}-create-for-limit-return-single", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['count'].should eq(nil)
        doc.parsed_response['result'].length.should eq(2)
        doc.parsed_response['cached'].should eq(false)
      end

      it "creates a cursor single run, large batch size" do
        cmd = api
        body = "{ \"query\" : \"FOR u IN #{@cn} LIMIT 2 RETURN u.n\", \"count\" : true, \"batchSize\" : 5 }"
        doc = ArangoDB.log_post("#{prefix}-create-for-limit-return-single-larger", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['count'].should eq(2)
        doc.parsed_response['result'].length.should eq(2)
        doc.parsed_response['cached'].should eq(false)
      end

      it "creates a usable cursor" do
        cmd = api
        body = "{ \"query\" : \"FOR u IN #{@cn} LIMIT 5 RETURN u.n\", \"count\" : true, \"batchSize\" : 2 }"
        doc = ArangoDB.log_post("#{prefix}-create-for-limit-return", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(5)
        doc.parsed_response['result'].length.should eq(2)
        doc.parsed_response['cached'].should eq(false)

        id = doc.parsed_response['id']

        cmd = api + "/#{id}"
        doc = ArangoDB.log_put("#{prefix}-create-for-limit-return-cont", cmd)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['id'].should eq(id)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(5)
        doc.parsed_response['result'].length.should eq(2)
        doc.parsed_response['cached'].should eq(false)

        cmd = api + "/#{id}"
        doc = ArangoDB.log_put("#{prefix}-create-for-limit-return-cont2", cmd)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['count'].should eq(5)
        doc.parsed_response['result'].length.should eq(1)
        doc.parsed_response['cached'].should eq(false)

        cmd = api + "/#{id}"
        doc = ArangoDB.log_put("#{prefix}-create-for-limit-return-cont3", cmd)
        
        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1600)
        doc.parsed_response['code'].should eq(404)
      end

      it "creates a cursor and deletes it in the middle" do
        cmd = api
        body = "{ \"query\" : \"FOR u IN #{@cn} LIMIT 5 RETURN u.n\", \"count\" : true, \"batchSize\" : 2 }"
        doc = ArangoDB.log_post("#{prefix}-create-for-limit-return", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(5)
        doc.parsed_response['result'].length.should eq(2)
        doc.parsed_response['cached'].should eq(false)

        id = doc.parsed_response['id']

        cmd = api + "/#{id}"
        doc = ArangoDB.log_put("#{prefix}-create-for-limit-return-cont", cmd)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['id'].should eq(id)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(5)
        doc.parsed_response['result'].length.should eq(2)
        doc.parsed_response['cached'].should eq(false)

        cmd = api + "/#{id}"
        doc = ArangoDB.log_delete("#{prefix}-delete", cmd)

        doc.code.should eq(202)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(202)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
      end

      it "deleting a cursor" do
        cmd = api
        body = "{ \"query\" : \"FOR u IN #{@cn} LIMIT 5 RETURN u.n\", \"count\" : true, \"batchSize\" : 2 }"
        doc = ArangoDB.post(cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(5)
        doc.parsed_response['result'].length.should eq(2)
        doc.parsed_response['cached'].should eq(false)

        id = doc.parsed_response['id']

        cmd = api + "/#{id}"
        doc = ArangoDB.log_delete("#{prefix}-delete", cmd)

        doc.code.should eq(202)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(202)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
      end

      it "deleting a deleted cursor" do
        cmd = api
        body = "{ \"query\" : \"FOR u IN #{@cn} LIMIT 5 RETURN u.n\", \"count\" : true, \"batchSize\" : 2 }"
        doc = ArangoDB.post(cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(5)
        doc.parsed_response['result'].length.should eq(2)
        doc.parsed_response['cached'].should eq(false)

        id = doc.parsed_response['id']

        cmd = api + "/#{id}"
        doc = ArangoDB.log_delete("#{prefix}-delete", cmd)

        doc.code.should eq(202)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(202)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        
        doc = ArangoDB.log_delete("#{prefix}-delete", cmd)

        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1600);
        doc.parsed_response['code'].should eq(404)
        doc.parsed_response['id'].should be_nil
      end

      it "deleting an invalid cursor" do
        cmd = api
        cmd = api + "/999999" # we assume this cursor id is invalid
        doc = ArangoDB.log_delete("#{prefix}-delete", cmd)

        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true);
        doc.parsed_response['errorNum'].should eq(1600);
        doc.parsed_response['code'].should eq(404)
        doc.parsed_response['id'].should be_nil
      end

      it "creates a cursor that will expire" do
        cmd = api
        body = "{ \"query\" : \"FOR u IN #{@cn} LIMIT 5 RETURN u.n\", \"count\" : true, \"batchSize\" : 1, \"ttl\" : 5 }"
        doc = ArangoDB.log_post("#{prefix}-create-ttl", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(5)
        doc.parsed_response['result'].length.should eq(1)
        doc.parsed_response['cached'].should eq(false)

        sleep 1
        id = doc.parsed_response['id']

        cmd = api + "/#{id}"
        doc = ArangoDB.log_put("#{prefix}-create-ttl", cmd)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['id'].should eq(id)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(5)
        doc.parsed_response['result'].length.should eq(1)
        doc.parsed_response['cached'].should eq(false)

        sleep 1

        doc = ArangoDB.log_put("#{prefix}-create-ttl", cmd)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(id)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(5)
        doc.parsed_response['result'].length.should eq(1)
        doc.parsed_response['cached'].should eq(false)

        # after this, the cursor might expire eventually
        # the problem is that we cannot exactly determine the point in time
        # when it really vanishes, as this depends on thread scheduling, state     
        # of the cleanup thread etc.

        sleep 8 # this should delete the cursor on the server
        doc = ArangoDB.log_put("#{prefix}-create-ttl", cmd)
        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1600)
        doc.parsed_response['code'].should eq(404)
      end
      
      it "creates a cursor that will not expire" do
        cmd = api
        body = "{ \"query\" : \"FOR u IN #{@cn} LIMIT 5 RETURN u.n\", \"count\" : true, \"batchSize\" : 1, \"ttl\" : 60 }"
        doc = ArangoDB.log_post("#{prefix}-create-ttl", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(5)
        doc.parsed_response['result'].length.should eq(1)
        doc.parsed_response['cached'].should eq(false)

        sleep 1
        id = doc.parsed_response['id']

        cmd = api + "/#{id}"
        doc = ArangoDB.log_put("#{prefix}-create-ttl", cmd)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['id'].should eq(id)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(5)
        doc.parsed_response['result'].length.should eq(1)
        doc.parsed_response['cached'].should eq(false)

        sleep 1

        doc = ArangoDB.log_put("#{prefix}-create-ttl", cmd)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(id)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(5)
        doc.parsed_response['result'].length.should eq(1)
        doc.parsed_response['cached'].should eq(false)

        sleep 5 # this should not delete the cursor on the server
        doc = ArangoDB.log_put("#{prefix}-create-ttl", cmd)
        
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(id)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(5)
        doc.parsed_response['result'].length.should eq(1)
        doc.parsed_response['cached'].should eq(false)
      end

      it "creates a query that executes a v8 expression during query optimization" do
        cmd = api
        body = "{ \"query\" : \"RETURN CONCAT('foo', 'bar', 'baz')\" }"
        doc = ArangoDB.log_post("#{prefix}-create-v8", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(1)
        doc.parsed_response['cached'].should eq(false)
      end

      it "creates a query that executes a v8 expression during query execution" do
        cmd = api
        body = "{ \"query\" : \"FOR u IN #{@cn} RETURN PASSTHRU(KEEP(u, '_key'))\" }"
        doc = ArangoDB.log_post("#{prefix}-create-v8", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(10)
        doc.parsed_response['cached'].should eq(false)
      end
      
      it "creates a query that executes a dynamic index expression during query execution" do
        cmd = api
        body = "{ \"query\" : \"FOR i IN #{@cn} FOR j IN #{@cn} FILTER i._key == j._key RETURN i._key\" }"
        doc = ArangoDB.log_post("#{prefix}-index-expression", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(10)
        doc.parsed_response['cached'].should eq(false)
      end
      
      it "creates a query that executes a dynamic V8 index expression during query execution" do
        cmd = api
        body = "{ \"query\" : \"FOR i IN #{@cn} FOR j IN #{@cn} FILTER j._key == PASSTHRU(i._key) RETURN i._key\" }"
        doc = ArangoDB.log_post("#{prefix}-v8-index-expression", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(10)
        doc.parsed_response['cached'].should eq(false)
      end

      it "creates a cursor with different bind values" do
        cmd = api
        body = "{ \"query\" : \"RETURN @values\", \"bindVars\" : { \"values\" : [ null, false, true, -1, 2.5, 3e4, \"\", \" \", \"true\", \"foo bar baz\", [ 1, 2, 3, \"bar\" ], { \"foo\" : \"bar\", \"\" : \"baz\", \" bar-baz \" : \"foo-bar\" } ] } }"
        doc = ArangoDB.log_post("#{prefix}-test-bind-values", cmd, :body => body)
        
        values = [ [ nil, false, true, -1, 2.5, 3e4, "", " ", "true", "foo bar baz", [ 1, 2, 3, "bar" ], { "foo" => "bar", "" => "baz", " bar-baz " => "foo-bar" } ] ]
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].should eq(values)
        doc.parsed_response['cached'].should eq(false)
      end
      
      it "calls wrong export API" do
        cmd = api
        body = "{ \"query\" : \"FOR u IN #{@cn} LIMIT 5 RETURN u.n\", \"count\" : true, \"batchSize\" : 2 }"
        doc = ArangoDB.log_post("#{prefix}-create-wrong-api", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['id'].should match(@reId)
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['count'].should eq(5)
        doc.parsed_response['result'].length.should eq(2)
        doc.parsed_response['cached'].should eq(false)

        id = doc.parsed_response['id']

        cmd = "/_api/export/#{id}"
        doc = ArangoDB.log_put("#{prefix}-create-wrong-api", cmd)
       
        if doc.code == 404  
          doc.code.should eq(404)
          doc.headers['content-type'].should eq("application/json; charset=utf-8")
          doc.parsed_response['error'].should eq(true)
          doc.parsed_response['code'].should eq(404)
          doc.parsed_response['errorNum'].should eq(1600)
        end
      end

      it "creates a query that survives memory limit constraints" do
        cmd = api
        body = "{ \"query\" : \"FOR i IN 1..10000 SORT i RETURN i\", \"memoryLimit\" : 10000000, \"batchSize\": 10 }"
        doc = ArangoDB.log_post("#{prefix}-memory-limit-ok", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should_not be_nil
        doc.parsed_response['hasMore'].should eq(true)
        doc.parsed_response['result'].length.should eq(10)
        doc.parsed_response['cached'].should eq(false)
      end

    end

################################################################################
## checking a query
################################################################################

    context "checking a query:" do
      before do
        @cn = "users"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn, false)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "valid query" do
        cmd = "/_api/query"
        body = "{ \"query\" : \"FOR u IN #{@cn} FILTER u.name == @name LIMIT 2 RETURN u.n\" }"
        doc = ArangoDB.log_post("api-query-valid", cmd, :body => body)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['bindVars'].should eq(["name"])
      end

      it "invalid query" do
        cmd = "/_api/query"
        body = "{ \"query\" : \"FOR u IN #{@cn} FILTER u.name = @name LIMIT 2 RETURN u.n\" }"
        doc = ArangoDB.log_post("api-query-invalid", cmd, :body => body)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1501)
      end
      
    end

################################################################################
## floating point values
################################################################################

    context "fetching floating-point values:" do
      before do
        @cn = "users"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn, false)

        ArangoDB.post("/_api/document?collection=#{@cid}", :body => "{ \"_key\" : \"big\", \"value\" : 4e+262 }")
        ArangoDB.post("/_api/document?collection=#{@cid}", :body => "{ \"_key\" : \"neg\", \"value\" : -4e262 }")
        ArangoDB.post("/_api/document?collection=#{@cid}", :body => "{ \"_key\" : \"pos\", \"value\" : 4e262 }")
        ArangoDB.post("/_api/document?collection=#{@cid}", :body => "{ \"_key\" : \"small\", \"value\" : 4e-262 }")
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "fetching via cursor" do
        cmd = api
        body = "{ \"query\" : \"FOR u IN #{@cn} SORT u._key RETURN u.value\" }"
        doc = ArangoDB.log_post("#{prefix}-float", cmd, :body => body)
        
        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['id'].should be_nil
        result = doc.parsed_response['result']
        result.length.should eq(4)
        result[0].should eq(4e262);
        result[1].should eq(-4e262);
        result[2].should eq(4e262);
        result[3].should eq(4e-262);
        
        doc = ArangoDB.get("/_api/document/#{@cid}/big")
        doc.parsed_response['value'].should eq(4e262)
        
        doc = ArangoDB.get("/_api/document/#{@cid}/neg")
        doc.parsed_response['value'].should eq(-4e262)

        doc = ArangoDB.get("/_api/document/#{@cid}/pos")
        doc.parsed_response['value'].should eq(4e262)

        doc = ArangoDB.get("/_api/document/#{@cid}/small")
        doc.parsed_response['value'].should eq(4e-262)
      end
    end

  end
end
