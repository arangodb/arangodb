# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do

  context "dealing with the admin interface:" do

    before do
      # load the most current routing information
      cmd = "/_admin/routing/reload"
      doc = ArangoDB.get(cmd)
    end

################################################################################
## load some routing
################################################################################

    context "dealing with the routing" do
      before do
        @id = nil
      end

      after do
        if @id != nil 
          ArangoDB.delete("/_api/document/" + @id)
        end 
      end
      
      it "checks a simple routing" do
        cmd = "/_api/document?collection=_routing"
        body = "{ \"url\" : { \"match\" : \"\/hello\/world\" }, \"content\" : { \"contentType\" : \"text\/html\", \"body\": \"moo!\" } }"
        doc = ArangoDB.log_post("api-routing-simple", cmd, :body => body)

        doc.code.should eq(202)
        @id = doc.parsed_response['_id']

        cmd = "/_admin/routing/reload"
        doc = ArangoDB.log_get("api-routing-simple", cmd)
     
        # we need this because reloading the routing is asynchronous 
        sleep(3)
             
        cmd = "/hello/world"
        doc = ArangoDB.log_get("api-routing-simple", cmd, :format => :plain)

        # check response code
        doc.code.should eq(200)
        # check whether HTML result contains expected title
        doc.response.body.should include("moo!")
      end

    end

################################################################################
## /_admin/echo
################################################################################

    context "checks /_admin/echo" do

      prefix = "api-system"

      it "using GET" do
        cmd = "/_admin/echo"
        doc = ArangoDB.log_get("#{prefix}-echo", cmd)

        doc.code.should eq(200)
        doc.parsed_response['url'].should eq("/_admin/echo")
        doc.parsed_response['path'].should eq("/")
        doc.parsed_response['parameters'].should eq({})
        doc.parsed_response['requestType'].should eq("GET")
      end
      
      it "using GET, with URL parameter" do
        cmd = "/_admin/echo?a=1"
        doc = ArangoDB.log_get("#{prefix}-echo", cmd)

        doc.code.should eq(200)
        doc.parsed_response['url'].should eq("/_admin/echo?a=1")
        doc.parsed_response['path'].should eq("/")
        doc.parsed_response['parameters'].should eq({ "a" => "1" })
        doc.parsed_response['requestType'].should eq("GET")
      end
      
      it "using POST, with URL parameters" do
        cmd = "/_admin/echo?a=1&b=2&foo[]=bar&foo[]=baz"
        body = "{\"foo\": \"bar\", \"baz\": { \"bump\": true, \"moo\": [ ] } }"
        doc = ArangoDB.log_post("#{prefix}-echo", cmd, :body => body)

        doc.code.should eq(200)
        doc.parsed_response['url'].should eq("/_admin/echo?a=1&b=2&foo[]=bar&foo[]=baz")
        doc.parsed_response['path'].should eq("/")
        doc.parsed_response['parameters'].should eq( { "a"=>"1", "b"=>"2", "foo"=>["bar", "baz"] } )
        doc.parsed_response['requestType'].should eq("POST")
        doc.parsed_response['requestBody'].should eq("{\"foo\": \"bar\", \"baz\": { \"bump\": true, \"moo\": [ ] } }")
      end
      
      it "using PUT, with headers" do
        cmd = "/_admin/echo?"
        body = "{ }"
        headers = { "X-Foo" => "Bar", "x-meow" => "mOO" }
        doc = ArangoDB.log_put("#{prefix}-echo", cmd, :body => body, :headers => headers)

        doc.code.should eq(200)
        doc.parsed_response['url'].should eq("/_admin/echo?")
        doc.parsed_response['path'].should eq("/")
        doc.parsed_response['parameters'].should eq({ })
        doc.parsed_response['requestType'].should eq("PUT")
        doc.parsed_response['requestBody'].should eq("{ }") 
      end
      
    end

################################################################################
## check whether admin interface is accessible
################################################################################
  
    context "dealing with the admin interface:" do
  
      it "checks whether the admin interface is available at /_admin/html/index.html" do
        cmd = "/_admin/html/index.html"
        doc = ArangoDB.log_get("admin-interface-get", cmd, :format => :plain)

        # check response code
        doc.code.should eq(200)
        # check whether HTML result contains expected title
        doc.response.body.should include("<!-- ArangoDB web interface -->")
      end

      it "checks whether the admin interface is available at /_admin/html" do
        cmd = "/_admin/html"
        begin
          doc = ArangoDB.log_get("admin-interface-get", cmd, :format => :plain, :no_follow => true)
        rescue HTTParty::RedirectionTooDeep => e
          # check response code
          e.response.code.should eq("301")
          e.response.header['location'].should eq("/_admin/html/index.html")
        end
      end

      it "checks whether the admin interface is available at /_admin/html/" do
        cmd = "/_admin/html/"
        begin
          doc = ArangoDB.log_get("admin-interface-get", cmd, :format => :plain, :no_follow => true)
        rescue HTTParty::RedirectionTooDeep => e
          # check response code
          e.response.code.should eq("301")
          e.response.header['location'].should eq("/_admin/html/index.html")
        end
      end
    
      it "checks whether the admin interface is available at /" do
        cmd = "/"
        begin
          doc = ArangoDB.log_get("admin-interface-get", cmd, :format => :plain, :no_follow => true)
        rescue HTTParty::RedirectionTooDeep => e
          # check response code
          e.response.code.should eq("301")
          e.response.header['location'].should eq("/_admin/html/index.html")
        end
      end
    end
    
  end

end
