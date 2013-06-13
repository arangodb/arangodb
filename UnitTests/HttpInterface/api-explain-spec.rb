# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do
  api = "/_api/explain"
  prefix = "api-explain"

  context "dealing with explain:" do

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
      end

      it "returns an error if collection is unknown" do
        cmd = api
        body = "{ \"query\" : \"FOR u IN unknowncollection LIMIT 2 RETURN u.n\" }"
        doc = ArangoDB.log_post("#{prefix}-unknown-collection", cmd, :body => body)
        
        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
        doc.parsed_response['errorNum'].should eq(1203)
      end

      it "returns an error if bind variables are missing completely" do
        cmd = api
        body = "{ \"query\" : \"FOR u IN [1,2] FILTER u.id == @id RETURN 1\" }"
        doc = ArangoDB.log_post("#{prefix}-missing-bind-variables-completely", cmd, :body => body)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1551)
      end
      
      it "returns an error if bind variables are required but empty" do
        cmd = api
        body = "{ \"query\" : \"FOR u IN [1,2] FILTER u.id == @id RETURN 1\", \"bindVars\" : { } }"
        doc = ArangoDB.log_post("#{prefix}-missing-bind-variables-empty", cmd, :body => body)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1551)
      end
      
      it "returns an error if bind variables are missing" do
        cmd = api
        body = "{ \"query\" : \"FOR u IN [1,2] FILTER u.id == @id RETURN 1\", \"bindVars\" : { \"id2\" : 1 } }"
        doc = ArangoDB.log_post("#{prefix}-missing-bind-variables-wrong", cmd, :body => body)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1551)
      end

      it "returns an error if query contains a parse error" do
        cmd = api
        body = "{ \"query\" : \"FOR u IN \" }"
        doc = ArangoDB.log_post("#{prefix}-parse-error", cmd, :body => body)
        
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1501)
      end

    end

################################################################################
## explaining
################################################################################

    context "explaining queries:" do
      it "explains a simple query" do
        cmd = api
        body = "{ \"query\" : \"FOR u IN [1,2] RETURN u\" }"
        doc = ArangoDB.log_post("#{prefix}-query-simple", cmd, :body => body)
        
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
      end
      
    end

  end
end
