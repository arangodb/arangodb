# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  prefix = "api-database"
  api = "/_api/database"

  context "dealing with database information methods:" do

################################################################################
## retrieving the list of databases
################################################################################

    it "retrieves the list of databases" do
      doc = ArangoDB.log_get("#{prefix}-list", api)

      doc.code.should eq(200)
      result = doc.parsed_response["result"]

      result.should include("_system")
    end

################################################################################
## retrieving the list of databases for the current user
################################################################################

    it "retrieves the list of user-specific databases" do
      doc = ArangoDB.log_get("#{prefix}-list-user", api + "/user")

      doc.code.should eq(200)
      result = doc.parsed_response["result"]

      result.should include("_system")
    end

################################################################################
## checking information about current database
################################################################################

    it "retrieves information about the current database" do
      doc = ArangoDB.log_get("#{prefix}-get-current", api + "/current")
      
      doc.code.should eq(200)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      result = doc.parsed_response["result"]
      result["name"].should eq("_system")
      result["path"].should be_kind_of(String)
      result["isSystem"].should eq(true)
    end

  end

################################################################################
## checking information about current database
################################################################################

  context "dealing with database manipulation methods:" do
    name = "UnitTestsDatabase"
        
    before do
      ArangoDB.delete(api + "/#{name}")
    end
    
    after do
      ArangoDB.delete(api + "/#{name}")
    end
    
    it "creates a new database" do
      body = "{\"name\" : \"#{name}\" }"
      doc = ArangoDB.log_post("#{prefix}-create", api, :body => body)
     
      doc.code.should eq(201)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      response = doc.parsed_response
      response["result"].should eq(true)
      response["error"].should eq(false)
    end
    
    it "creates a database without a name" do
      body = "{ }"
      doc = ArangoDB.log_post("#{prefix}-create-invalid-non", api, :body => body)
     
      doc.code.should eq(400)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      response = doc.parsed_response
      response["error"].should eq(true)
      response["errorNum"].should eq(1229)
    end
    
    it "creates a database with an empty name" do
      body = "{ \"name\": \" \" }"
      doc = ArangoDB.log_post("#{prefix}-create-invalid-empty", api, :body => body)
     
      doc.code.should eq(400)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      response = doc.parsed_response
      response["error"].should eq(true)
      response["errorNum"].should eq(1229)
    end
    
    it "creates a database with an invalid name" do
      body = "{\"name\" : \"_#{name}\" }"
      doc = ArangoDB.log_post("#{prefix}-create-invalid-system", api, :body => body)
     
      doc.code.should eq(400)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      response = doc.parsed_response
      response["error"].should eq(true)
      response["errorNum"].should eq(1229)
    end
    
    it "re-creates an existing database" do
      body = "{\"name\" : \"#{name}\" }"
      doc = ArangoDB.log_post("#{prefix}-re-create", api, :body => body)
     
      doc.code.should eq(201)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      
      doc = ArangoDB.log_post("#{prefix}-re-create", api, :body => body)
      doc.code.should eq(409)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      
      response = doc.parsed_response
      response["error"].should eq(true)
      response["errorNum"].should eq(1207)
    end
    
    it "creates a database with users = null" do
      body = "{\"name\" : \"#{name}\", \"users\" : null }"
      doc = ArangoDB.log_post("#{prefix}-create-no-users1", api, :body => body)
     
      doc.code.should eq(201)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      response = doc.parsed_response
      response["result"].should eq(true)
      response["error"].should eq(false)
    end
    
    it "creates a database with users = [ ]" do
      body = "{\"name\" : \"#{name}\", \"users\" : [ ] }"
      doc = ArangoDB.log_post("#{prefix}-create-no-users2", api, :body => body)
     
      doc.code.should eq(201)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      response = doc.parsed_response
      response["result"].should eq(true)
      response["error"].should eq(false)
    end
    
    it "drops an existing database" do
      cmd = api + "/#{name}"
      body = "{\"name\" : \"#{name}\" }"
      doc = ArangoDB.log_post("#{prefix}-drop", api, :body => body)

      doc.code.should eq(201)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      
      doc = ArangoDB.log_delete("#{prefix}-drop", cmd)
      doc.code.should eq(200)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      response = doc.parsed_response
      response["result"].should eq(true)
      response["error"].should eq(false)
    end
    
    it "drops a non-existing database" do
      cmd = api + "/#{name}"
      doc = ArangoDB.log_delete("#{prefix}-drop-nonexisting", cmd)
     
      doc.code.should eq(404)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      response = doc.parsed_response
      response["errorNum"].should eq(1228)
      response["error"].should eq(true)
    end
    
    it "creates a new database and retrieves the properties" do
      body = "{\"name\" : \"#{name}\" }"
      doc = ArangoDB.log_post("#{prefix}-create-properties", api, :body => body)
     
      doc.code.should eq(201)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      response = doc.parsed_response
      response["result"].should eq(true)
      response["error"].should eq(false)

      # list of databases should include the new database
      doc = ArangoDB.log_get("#{prefix}-create-properties", api)
      doc.code.should eq(200)
      result = doc.parsed_response["result"]

      result.should include("_system")
      result.should include(name)

      # retrieve information about _system database
      doc = ArangoDB.log_get("#{prefix}-create-current", api + "/current")
      doc.code.should eq(200)
      result = doc.parsed_response["result"]
      result["name"].should eq("_system")
      result["path"].should be_kind_of(String)
      result["isSystem"].should eq(true)

      # retrieve information about new database
      doc = ArangoDB.log_get("#{prefix}-create-current", "/_db/#{name}" + api + "/current")
      doc.code.should eq(200)
      result = doc.parsed_response["result"]
      result["name"].should eq(name)
      result["path"].should be_kind_of(String)
      result["isSystem"].should eq(false)
      
      doc = ArangoDB.log_delete("#{prefix}-create-current", api + "/#{name}")
      doc.code.should eq(200)
      response = doc.parsed_response
      response["result"].should eq(true)
      response["error"].should eq(false)
    end
    
    it "creates a new database with two users" do
      body = "{\"name\" : \"#{name}\", \"users\": [ { \"username\": \"admin\", \"password\": \"secret\", \"extra\": { \"gender\": \"m\" } }, { \"username\": \"foxx\", \"active\": false } ] }"
      doc = ArangoDB.log_post("#{prefix}-create-users", api, :body => body)
     
      doc.code.should eq(201)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      response = doc.parsed_response
      response["result"].should eq(true)
      response["error"].should eq(false)

      # list of databases should include the new database
      doc = ArangoDB.log_get("#{prefix}-create-users", api)
      doc.code.should eq(200)
      result = doc.parsed_response["result"]

      result.should include("_system")
      result.should include(name)

      # retrieve information about new database
      doc = ArangoDB.log_get("#{prefix}-create-users", "/_db/#{name}" + api + "/current")
      doc.code.should eq(200)
      result = doc.parsed_response["result"]
      result["name"].should eq(name)
      result["path"].should be_kind_of(String)
      result["isSystem"].should eq(false)
      
      # retrieve information about user "admin"
      doc = ArangoDB.log_get("#{prefix}-create-users", "/_db/_system/_api/user/admin")
      doc.code.should eq(200)
      result = doc.parsed_response
      result["user"].should eq("admin")
      result["active"].should eq(true)
      result["extra"]["gender"].should eq("m")
      
      # retrieve information about user "foxx"
      doc = ArangoDB.log_get("#{prefix}-create-users", "/_db/_system/_api/user/foxx")
      doc.code.should eq(200)
      result = doc.parsed_response
      result["user"].should eq("foxx")
      result["active"].should eq(false)
      
      doc = ArangoDB.log_delete("#{prefix}-create-users", api + "/#{name}")
      doc.code.should eq(200)
      response = doc.parsed_response
      response["result"].should eq(true)
      response["error"].should eq(false)
    end
    
    it "creates a new database with two users, using 'user' attribute" do
      body = "{\"name\" : \"#{name}\", \"users\": [ { \"user\": \"admin\", \"password\": \"secret\", \"extra\": { \"gender\": \"m\" } }, { \"user\": \"foxx\", \"active\": false } ] }"
      doc = ArangoDB.log_post("#{prefix}-create-users", api, :body => body)
     
      doc.code.should eq(201)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      response = doc.parsed_response
      response["result"].should eq(true)
      response["error"].should eq(false)

      # list of databases should include the new database
      doc = ArangoDB.log_get("#{prefix}-create-users", api)
      doc.code.should eq(200)
      result = doc.parsed_response["result"]

      result.should include("_system")
      result.should include(name)

      # retrieve information about new database
      doc = ArangoDB.log_get("#{prefix}-create-users", "/_db/#{name}" + api + "/current")
      doc.code.should eq(200)
      result = doc.parsed_response["result"]
      result["name"].should eq(name)
      result["path"].should be_kind_of(String)
      result["isSystem"].should eq(false)
      
      # retrieve information about user "admin"
      doc = ArangoDB.log_get("#{prefix}-create-users", "/_db/_system/_api/user/admin")
      doc.code.should eq(200)
      result = doc.parsed_response
      result["user"].should eq("admin")
      result["active"].should eq(true)
      result["extra"]["gender"].should eq("m")
      
      # retrieve information about user "foxx"
      doc = ArangoDB.log_get("#{prefix}-create-users", "/_db/_system/_api/user/foxx")
      doc.code.should eq(200)
      result = doc.parsed_response
      result["user"].should eq("foxx")
      result["active"].should eq(false)
      
      doc = ArangoDB.log_delete("#{prefix}-create-users", api + "/#{name}")
      doc.code.should eq(200)
      response = doc.parsed_response
      response["result"].should eq(true)
      response["error"].should eq(false)
    end
    
    it "creates a new database with an invalid user object" do
      body = "{\"name\" : \"#{name}\", \"users\": [ { } ] }"
      doc = ArangoDB.log_post("#{prefix}-create-users-missing", api, :body => body)
     
      doc.code.should eq(400)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      response = doc.parsed_response
      response["error"].should eq(true)
    end
    
    it "creates a new database with an invalid user" do
      body = "{\"name\" : \"#{name}\", \"users\": [ { \"username\": \"\" } ] }"
      doc = ArangoDB.log_post("#{prefix}-create-users-invalid", api, :body => body)
     
      doc.code.should eq(201)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      response = doc.parsed_response
      response["result"].should eq(true)
      response["error"].should eq(false)

      # list of databases should include the new database
      doc = ArangoDB.log_get("#{prefix}-create-users-invalid", api)
      doc.code.should eq(200)
      result = doc.parsed_response["result"]

      result.should include("_system")
      result.should include(name)

      # retrieve information about new database
      doc = ArangoDB.log_get("#{prefix}-create-users-invalid", "/_db/#{name}" + api + "/current")
      doc.code.should eq(200)
      result = doc.parsed_response["result"]
      result["name"].should eq(name)
      result["path"].should be_kind_of(String)
      result["isSystem"].should eq(false)
      
      doc = ArangoDB.log_delete("#{prefix}-create-users-invalid", api + "/#{name}")
      doc.code.should eq(200)
      response = doc.parsed_response
      response["result"].should eq(true)
      response["error"].should eq(false)
    end
  end
end
