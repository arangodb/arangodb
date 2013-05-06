# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do
  api = "/_api/aqlfunction"
  prefix = "api-aqlfunction"

  context "AQL user functions:" do

################################################################################
## error handling 
################################################################################

    context "error handling" do

      it "add function, without name" do
        body = "{ \"code\" : \"function () { return 1; }\" }"
        doc = ArangoDB.log_post("#{prefix}-add-no-name", api, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1580)
      end
    
      it "add function, invalid name" do
        body = "{ \"name\" : \"\", \"code\" : \"function () { return 1; }\" }"
        doc = ArangoDB.log_post("#{prefix}-add-invalid1", api, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1580)
      end
      
      it "add function, invalid name" do
        body = "{ \"name\" : \"_aql:foobar\", \"code\" : \"function () { return 1; }\" }"
        doc = ArangoDB.log_post("#{prefix}-add-invalid2", api, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1580)
      end
      
      it "add function, invalid name" do
        body = "{ \"name\" : \"foobar\", \"code\" : \"function () { return 1; }\" }"
        doc = ArangoDB.log_post("#{prefix}-add-invalid3", api, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1580)
      end
      
      it "add function, no code" do
        body = "{ \"name\" : \"myfunc:mytest\" }"
        doc = ArangoDB.log_post("#{prefix}-add-no-code", api, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1581)
      end
      
      it "add function, invalid code" do
        body = "{ \"name\" : \"myfunc:mytest\", \"code\" : \"function ()\" }"
        doc = ArangoDB.log_post("#{prefix}-add-invalid-code", api, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1581)
      end
    
      it "deleting non-existing function" do
        doc = ArangoDB.log_delete("#{prefix}-delete", api + "/mytest%3Amynonfunc")

        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
        doc.parsed_response['errorNum'].should eq(1582)
      end
    end

################################################################################
## adding and deleting functions
################################################################################

    context "adding and deleting functions" do
      before do
        ArangoDB.delete("/_api/aqlfunction/UnitTests%3Amytest")
      end

      after do
        ArangoDB.delete("/_api/aqlfunction/UnitTests%3Amytest")
      end

      it "add function, valid code" do
        body = "{ \"name\" : \"UnitTests:mytest\", \"code\": \"function () { return 1; }\" }"
        doc = ArangoDB.log_post("#{prefix}-add-function1", api, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
      end
  
      it "add function, update" do
        body = "{ \"name\" : \"UnitTests:mytest\", \"code\": \"function () { return 1; }\" }"
        doc = ArangoDB.log_post("#{prefix}-add-function2", api, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        
        doc = ArangoDB.log_post("#{prefix}-add-function2", api, :body => body)
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
      end
      
      it "add function, delete" do
        body = "{ \"name\" : \"UnitTests:mytest\", \"code\": \"function () { return 1; }\" }"
        doc = ArangoDB.log_post("#{prefix}-add-function3", api, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        
        doc = ArangoDB.log_delete("#{prefix}-add-function3", api + "/UnitTests%3Amytest")
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        
        doc = ArangoDB.log_delete("#{prefix}-add-function3", api + "/UnitTests%3Amytest")
        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
        doc.parsed_response['errorNum'].should eq(1582)
      end
      
      it "add function, delete multiple" do
        body = "{ \"name\" : \"UnitTests:mytest:one\", \"code\": \"function () { return 1; }\" }"
        doc = ArangoDB.log_post("#{prefix}-add-function4", api, :body => body)
        doc.code.should eq(201)
        
        body = "{ \"name\" : \"UnitTests:mytest:two\", \"code\": \"function () { return 1; }\" }"
        doc = ArangoDB.log_post("#{prefix}-add-function4", api, :body => body)
        doc.code.should eq(201)
        
        body = "{ \"name\" : \"UnitTests:foo\", \"code\": \"function () { return 1; }\" }"
        doc = ArangoDB.log_post("#{prefix}-add-function4", api, :body => body)
        doc.code.should eq(201)
        
        doc = ArangoDB.log_delete("#{prefix}-add-function4", api + "/UnitTests%3Amytest?group=true")
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        
        doc = ArangoDB.log_delete("#{prefix}-add-function4", api + "/UnitTests%3Amytest%3Aone")
        doc.code.should eq(404)
        doc = ArangoDB.log_delete("#{prefix}-add-function4", api + "/UnitTests%3Amytest%3Atwo")
        doc.code.should eq(404)
        doc = ArangoDB.log_delete("#{prefix}-add-function4", api + "/UnitTests%3Afoo")
        doc.code.should eq(200)
      end
    end

################################################################################
## retrieving the list of functions
################################################################################

    context "retrieving functions" do
      before do
        ArangoDB.delete("/_api/aqlfunction/UnitTests?group=true")
      end

      after do
        ArangoDB.delete("/_api/aqlfunction/UnitTests?group=true")
      end

      it "add function and retrieve the list" do
        body = "{ \"name\" : \"UnitTests:mytest\", \"code\": \"function () { return 1; }\" }"
        doc = ArangoDB.log_post("#{prefix}-list-functions1", api, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        
        doc = ArangoDB.log_get("#{prefix}-list-functions1", api + "?prefix=UnitTests")
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response.length.should eq(1)
        doc.parsed_response[0]['name'].should eq("UnitTests:mytest")
        doc.parsed_response[0]['code'].should eq("function () { return 1; }")
      end
      
      it "add functions and retrieve the list" do
        body = "{ \"name\" : \"UnitTests:mytest1\", \"code\": \"function () { return 1; }\" }"
        doc = ArangoDB.log_post("#{prefix}-list-functions2", api, :body => body)
        doc.code.should eq(201)
        
        doc = ArangoDB.log_get("#{prefix}-list-functions2", api + "?prefix=UnitTests")
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response.length.should eq(1)
        doc.parsed_response[0]['name'].should eq("UnitTests:mytest1")
        doc.parsed_response[0]['code'].should eq("function () { return 1; }")

        body = "{ \"name\" : \"UnitTests:mytest1\", \"code\": \"( function () { return   3 * 5; } ) \" }"
        doc = ArangoDB.log_post("#{prefix}-list-functions2", api, :body => body)
        doc.code.should eq(200)
        
        doc = ArangoDB.log_get("#{prefix}-list-functions2", api + "?prefix=UnitTests")
        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response.length.should eq(1)
        doc.parsed_response[0]['name'].should eq("UnitTests:mytest1")
        doc.parsed_response[0]['code'].should eq("( function () { return   3 * 5; } ) ")
      end
  
    end

  end
end
    
