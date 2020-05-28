# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do

  context "dealing with the admin interface:" do

    before do
      # load the most current routing information
      cmd = "/_admin/routing/reload"
      ArangoDB.get(cmd)
    end

################################################################################
## load some routing
################################################################################

    context "dealing with the routing" do
      before do
        @id = nil
        # make sure system collection exists
        ArangoDB.post("/_admin/execute", :body => "var db = require('internal').db; try { db._create('_routing', { isSystem: true, distributeShardsLike: '_users' }); } catch (err) {}")   
      end

      after do
        if @id != nil 
          ArangoDB.delete("/_api/document/" + @id)
        end 
      
        # drop collection
        ArangoDB.post("/_admin/execute", :body => "var db = require('internal').db; try { db._drop('_routing', true); } catch (err) {}")   
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
## check whether admin interface is accessible
################################################################################
  
    context "dealing with the admin interface:" do

      it "checks whether the admin interface is available at /_admin/aardvark/index.html" do
        cmd = "/_admin/aardvark/index.html"
        begin 
          ArangoDB.log_get("admin-interface-get", cmd, :format => :plain, :no_follow => true)
        rescue HTTParty::RedirectionTooDeep => e
          # check response code
          e.response.code.should eq("302")
        end
      end

      it "checks whether the admin interface is available at /" do
        cmd = "/"
        begin
          ArangoDB.log_get("admin-interface-get", cmd, :format => :plain, :no_follow => true)
        rescue HTTParty::RedirectionTooDeep => e
          # check response code
          e.response.code.should eq("301")
          e.response.header['location'].should =~ /^\/.*\/*_admin\/aardvark\/index.html$/
        end
      end

      it "checks whether the admin interface is available at /_admin/html" do
        cmd = "/_admin/html"
        begin
          ArangoDB.log_get("admin-interface-get", cmd, :format => :plain, :no_follow => true)
        rescue HTTParty::RedirectionTooDeep => e
          # check response code
          e.response.code.should eq("301")
          e.response.header['location'].should =~ /\/_admin\/aardvark\/index.html$/
        end
      end

      it "checks whether the admin interface is available at /_admin/html/" do
        cmd = "/_admin/html/"
        begin
          ArangoDB.log_get("admin-interface-get", cmd, :format => :plain, :no_follow => true)
        rescue HTTParty::RedirectionTooDeep => e
          # check response code
          e.response.code.should eq("301")
          e.response.header['location'].should =~ /\/_admin\/aardvark\/index.html$/
        end
      end
      
      it "checks whether the admin interface is available at /_admin/aardvark/" do
        cmd = "/_admin/aardvark/"
        begin
          ArangoDB.log_get("admin-interface-get", cmd, :format => :plain, :no_follow => true)
        rescue HTTParty::RedirectionTooDeep => e
          # check response code
          e.response.code.should eq("307")
          e.response.header['location'].should =~ /\/_admin\/aardvark\/index.html$/
        end
      end
    
    end
    
  end

end
