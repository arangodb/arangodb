# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_api/collection"
  prefix = "api-collection"

  context "dealing with collections:" do

################################################################################
## reading all collections
################################################################################

    context "all collections:" do
      before do
        for cn in ["units", "employees", "locations" ] do
          ArangoDB.drop_collection(cn)
          @cid = ArangoDB.create_collection(cn)
        end
      end

      after do
        for cn in ["units", "employees", "locations" ] do
          ArangoDB.drop_collection(cn)
        end
      end

      it "returns all collections" do
        cmd = api
        doc = ArangoDB.log_get("#{prefix}-all-collections", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)

        collections = doc.parsed_response["result"];

        total = 0
        realCollections = [ ]
        collections.each { |collection|
          if [ "units", "employees", "locations" ].include? collection["name"]
            realCollections.push(collection)
          end
          total = total + 1
        }

        realCollections.length.should eq(3)
        total.should be > 3
      end

      it "returns all collections, exclude system collections" do
        cmd = api + '/?excludeSystem=true'
        doc = ArangoDB.log_get("#{prefix}-all-collections-nosystem", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        
        collections = doc.parsed_response["result"]
        realCollections = [ ]

        total = 0
        collections.each { |collection|
          if [ "units", "employees", "locations" ].include? collection["name"]
            realCollections.push(collection)
          end
          total = total + 1
        }

        realCollections.length.should eq(3)
        total.should >= 3
      end

    end

################################################################################
## error handling
################################################################################

    context "error handling:" do
      it "returns an error if collection identifier is unknown" do
        cmd = api + "/123456"
        doc = ArangoDB.log_get("#{prefix}-bad-identifier", cmd)

        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1203)
        doc.parsed_response['code'].should eq(404)
      end

      it "creating a collection without name" do
        cmd = api
        doc = ArangoDB.log_post("#{prefix}-create-missing-name", cmd)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1208)
      end

      it "creating a collection with an illegal name" do
        cmd = api
        body = "{ \"name\" : \"1\" }"
        doc = ArangoDB.log_post("#{prefix}-create-illegal-name", cmd, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1208)
      end

      it "creating a collection with a duplicate name" do
        cn = "UnitTestsCollectionBasics"
        cid = ArangoDB.create_collection(cn)

        cmd = api
        body = "{ \"name\" : \"#{cn}\" }"
              doc = ArangoDB.log_post("#{prefix}-create-illegal-name", cmd, :body => body)

        doc.code.should eq(409)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(409)
        doc.parsed_response['errorNum'].should eq(1207)
      end

      it "creating a collection with an illegal body" do
        cmd = api
        body = "{ name : world }"
              doc = ArangoDB.log_post("#{prefix}-create-illegal-body", cmd, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(600)
        doc.parsed_response['errorMessage'].should eq("SyntaxError: Unexpected token n in JSON at position 2")
      end

      it "creating a collection with a null body" do
        cmd = api
        body = "null"
              doc = ArangoDB.log_post("#{prefix}-create-null-body", cmd, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1208)
      end
    end

################################################################################
## reading a collection
################################################################################

    context "reading:" do
      before do
        @cn = "UnitTestsCollectionBasics"
        ArangoDB.drop_collection(@cn)
        @cid = ArangoDB.create_collection(@cn)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      # get
      it "finds the collection by identifier" do
        cmd = api + "/" + String(@cid)
        doc = ArangoDB.log_get("#{prefix}-get-collection-identifier", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(@cid)
        doc.parsed_response['name'].should eq(@cn)
        doc.parsed_response['status'].should eq(3)

        cmd2 = api + "/" + @cn + "/unload"
        doc = ArangoDB.put(cmd2)

        doc = ArangoDB.log_get("#{prefix}-get-collection-identifier", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(@cid)
        doc.parsed_response['name'].should eq(@cn)
        [2, 4].should include(doc.parsed_response['status'])
      end

      # get
      it "finds the collection by name" do
        cmd = api + "/" + @cn
        doc = ArangoDB.log_get("#{prefix}-get-collection-name", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(@cid)
        doc.parsed_response['name'].should eq(@cn)
        doc.parsed_response['status'].should eq(3)

        cmd2 = api + "/" + @cn + "/unload"
        doc = ArangoDB.put(cmd2)

        doc = ArangoDB.log_get("#{prefix}-get-collection-name", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(@cid)
        doc.parsed_response['name'].should eq(@cn)
        [2, 4].should include(doc.parsed_response['status'])
      end

      # get count
      it "checks the size of a collection" do
        cmd = api + "/" + @cn + "/count"
        doc = ArangoDB.log_get("#{prefix}-get-collection-count", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(@cid)
        doc.parsed_response['name'].should eq(@cn)
        doc.parsed_response['status'].should eq(3)
        doc.parsed_response['count'].should be_kind_of(Integer)
      end

      # get count
      it "checks the properties of a collection" do
        cmd = api + "/" + @cn + "/properties"
        doc = ArangoDB.log_get("#{prefix}-get-collection-properties", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(@cid)
        doc.parsed_response['name'].should eq(@cn)
        doc.parsed_response['status'].should eq(3)
        doc.parsed_response['doCompact'].should eq(true)
        doc.parsed_response['waitForSync'].should eq(true)
        doc.parsed_response['isVolatile'].should eq(false)
        doc.parsed_response['isSystem'].should eq(false)
        doc.parsed_response['journalSize'].should be_kind_of(Integer)
      end
      
      describe "figures", :timecritical => true do
        # get figures
        it "extracting the figures for a collection" do
          # flush wal
          ArangoDB.put("/_admin/wal/flush?waitForSync=true&waitForCollector=true", { })
          sleep 3

          cmd = api + "/" + @cn + "/figures"
          doc = ArangoDB.log_get("#{prefix}-get-collection-figures", cmd)

          doc.code.should eq(200)
          doc.headers['content-type'].should eq("application/json; charset=utf-8")
          doc.parsed_response['error'].should eq(false)
          doc.parsed_response['code'].should eq(200)
          doc.parsed_response['id'].should eq(@cid)
          doc.parsed_response['name'].should eq(@cn)
          doc.parsed_response['status'].should eq(3)
          doc.parsed_response['count'].should be_kind_of(Integer)
          doc.parsed_response['count'].should eq(0)
          doc.parsed_response['figures']['dead']['count'].should be_kind_of(Integer)
          doc.parsed_response['figures']['dead']['count'].should eq(0)
          doc.parsed_response['figures']['alive']['count'].should be_kind_of(Integer)
          doc.parsed_response['figures']['alive']['count'].should eq(0)
          doc.parsed_response['figures']['datafiles']['count'].should be_kind_of(Integer)
          doc.parsed_response['figures']['datafiles']['fileSize'].should be_kind_of(Integer)
          doc.parsed_response['figures']['datafiles']['count'].should eq(0)
          doc.parsed_response['figures']['journals']['count'].should be_kind_of(Integer)
          doc.parsed_response['figures']['journals']['fileSize'].should be_kind_of(Integer)
          doc.parsed_response['figures']['journals']['count'].should eq(0)
          doc.parsed_response['figures']['compactors']['count'].should be_kind_of(Integer)
          doc.parsed_response['figures']['compactors']['fileSize'].should be_kind_of(Integer)
          doc.parsed_response['figures']['compactors']['count'].should eq(0)
          doc.parsed_response['journalSize'].should be_kind_of(Integer)
                
          # create a few documents, this should increase counts
          (0...10).each{|i|
            body = "{ \"test\" : " + i.to_s + " }"
            doc = ArangoDB.log_post("#{prefix}-get-collection-figures", "/_api/document/?collection=" + @cn, :body => body)
          }

          # flush wal
          ArangoDB.put("/_admin/wal/flush?waitForSync=true&waitForCollector=true", { })
          sleep 6
          
          doc = ArangoDB.log_get("#{prefix}-get-collection-figures", cmd)
          doc.code.should eq(200)
          doc.headers['content-type'].should eq("application/json; charset=utf-8")
          doc.parsed_response['error'].should eq(false)
          doc.parsed_response['code'].should eq(200)
          doc.parsed_response['count'].should be_kind_of(Integer)
          doc.parsed_response['count'].should eq(10)
          doc.parsed_response['figures']['dead']['count'].should be_kind_of(Integer)
          doc.parsed_response['figures']['dead']['count'].should eq(0)
          doc.parsed_response['figures']['alive']['count'].should be_kind_of(Integer)
          doc.parsed_response['figures']['alive']['count'].should eq(10)
          doc.parsed_response['figures']['datafiles']['count'].should eq(0)
          doc.parsed_response['figures']['journals']['count'].should eq(1)

          # create a few different documents, this should increase counts
          (0...10).each{|i|
            body = "{ \"test" + i.to_s + "\" : 1 }"
            doc = ArangoDB.log_post("#{prefix}-get-collection-figures", "/_api/document/?collection=" + @cn, :body => body)
          }
          
          # flush wal
          ArangoDB.put("/_admin/wal/flush?waitForSync=true&waitForCollector=true", { })
          sleep 6
          
          doc = ArangoDB.log_get("#{prefix}-get-collection-figures", cmd)
          doc.code.should eq(200)
          doc.headers['content-type'].should eq("application/json; charset=utf-8")
          doc.parsed_response['error'].should eq(false)
          doc.parsed_response['code'].should eq(200)
          doc.parsed_response['count'].should be_kind_of(Integer)
          doc.parsed_response['count'].should eq(20)
          doc.parsed_response['figures']['dead']['count'].should be_kind_of(Integer)
          doc.parsed_response['figures']['dead']['count'].should eq(0)
          doc.parsed_response['figures']['alive']['count'].should be_kind_of(Integer)
          doc.parsed_response['figures']['alive']['count'].should eq(20)
          doc.parsed_response['figures']['datafiles']['count'].should eq(0)
          doc.parsed_response['figures']['journals']['count'].should eq(1)
          
          # delete a few documents, this should change counts
          body = "{ \"collection\" : \"" + @cn + "\", \"example\": { \"test\" : 5 } }"
          doc = ArangoDB.log_put("#{prefix}-get-collection-figures", "/_api/simple/remove-by-example", :body => body)
          body = "{ \"collection\" : \"" + @cn + "\", \"example\": { \"test3\" : 1 } }"
          doc = ArangoDB.log_put("#{prefix}-get-collection-figures", "/_api/simple/remove-by-example", :body => body)
          
          # flush wal
          ArangoDB.put("/_admin/wal/flush?waitForSync=true&waitForCollector=true", { })
          sleep 3
          
          doc = ArangoDB.log_get("#{prefix}-get-collection-figures", cmd)
          doc.code.should eq(200)
          doc.headers['content-type'].should eq("application/json; charset=utf-8")
          doc.parsed_response['error'].should eq(false)
          doc.parsed_response['code'].should eq(200)
          doc.parsed_response['count'].should be_kind_of(Integer)
          doc.parsed_response['count'].should eq(18)
          doc.parsed_response['figures']['dead']['count'].should be_kind_of(Integer)
          doc.parsed_response['figures']['dead']['count'].should eq(2)
          doc.parsed_response['figures']['alive']['count'].should be_kind_of(Integer)
          doc.parsed_response['figures']['alive']['count'].should eq(18)
          doc.parsed_response['figures']['datafiles']['count'].should eq(0)
          doc.parsed_response['figures']['journals']['count'].should eq(1)
        end
      end
      
      # get revision id
      it "extracting the revision id of a collection" do
        cmd = api + "/" + @cn + "/revision"
        doc = ArangoDB.log_get("#{prefix}-get-collection-revision", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(@cid)
        doc.parsed_response['name'].should eq(@cn)
        doc.parsed_response['status'].should eq(3)
        r1 = doc.parsed_response['revision']
        r1.should be_kind_of(String)
        r1.should_not eq("");

        # create a new document
        body = "{ \"test\" : 1 }"
        doc = ArangoDB.log_post("#{prefix}-get-collection-revision", "/_api/document/?collection=" + @cn, :body => body)
        
        # fetch revision again
        doc = ArangoDB.log_get("#{prefix}-get-collection-revision", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['revision'].should be_kind_of(String)

        r2 = doc.parsed_response['revision']
        r2.should_not eq("");
        r2.should_not eq(r1);
        
        # create another document
        doc = ArangoDB.log_post("#{prefix}-get-collection-revision", "/_api/document/?collection=" + @cn, :body => body)
        
        # fetch revision again
        doc = ArangoDB.log_get("#{prefix}-get-collection-revision", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['revision'].should be_kind_of(String)

        r3 = doc.parsed_response['revision']
        r3.should_not eq("");
        r3.should_not eq(r1);
        r3.should_not eq(r2);

        # truncate
        doc = ArangoDB.log_put("#{prefix}-get-collection-revision", "/_api/collection/#{@cn}/truncate", :body => "")
        
        # fetch revision again
        doc = ArangoDB.log_get("#{prefix}-get-collection-revision", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['revision'].should be_kind_of(String)

        r4 = doc.parsed_response['revision']
        r4.should_not eq("");
        r4.should_not eq(r1);
        r4.should_not eq(r2);
        r4.should_not eq(r3);
      end
    end

################################################################################
## deleting of collection
################################################################################

    context "deleting:" do
      before do
        @cn = "UnitTestsCollectionBasics"
      end

      it "delete an existing collection by identifier" do
        cid = ArangoDB.create_collection(@cn)
        cmd = api + "/" + @cn
        doc = ArangoDB.log_delete("#{prefix}-delete-collection-identifier", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(cid)

        cmd = api + "/" + @cn
        doc = ArangoDB.get(cmd)

        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
      end

      it "delete an existing collection by name" do
        cid = ArangoDB.create_collection(@cn)
        cmd = api + "/" + @cn
        doc = ArangoDB.log_delete("#{prefix}-delete-collection-name", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(cid)

        cmd = api + "/" + @cn
        doc = ArangoDB.get(cmd)

        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
      end
    end

################################################################################
## creating a collection
################################################################################

    context "creating:" do
      before do
        @cn = "UnitTestsCollectionBasics"
      end

      it "create a collection" do
        cmd = api
        body = "{ \"name\" : \"#{@cn}\" }"
        doc = ArangoDB.log_post("#{prefix}-create-collection", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should be_kind_of(String) 
        doc.parsed_response['name'].should eq(@cn)
        doc.parsed_response['waitForSync'].should eq(false)

        cmd = api + "/" + @cn + "/figures"
        doc = ArangoDB.get(cmd)

        doc.parsed_response['waitForSync'].should eq(false)

        ArangoDB.drop_collection(@cn)
      end

      it "create a collection, sync" do
        cmd = api
        body = "{ \"name\" : \"#{@cn}\", \"waitForSync\" : true }"
        doc = ArangoDB.log_post("#{prefix}-create-collection-sync", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['name'].should eq(@cn)
        doc.parsed_response['waitForSync'].should eq(true)

        cmd = api + "/" + @cn + "/figures"
        doc = ArangoDB.get(cmd)

        doc.parsed_response['waitForSync'].should eq(true)

        ArangoDB.drop_collection(@cn)
      end
      
      it "create a collection, volatile" do
        cmd = api
        body = "{ \"name\" : \"#{@cn}\", \"isVolatile\" : true }"
        doc = ArangoDB.log_post("#{prefix}-create-collection-volatile", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should be_kind_of(String)
        doc.parsed_response['name'].should eq(@cn)
        doc.parsed_response['waitForSync'].should eq(false)
        doc.parsed_response['isVolatile'].should eq(true)
        doc.parsed_response['isSystem'].should eq(false)

        cmd = api + "/" + @cn + "/figures"
        doc = ArangoDB.get(cmd)

        doc.parsed_response['waitForSync'].should eq(false)

        ArangoDB.drop_collection(@cn)
      end
            
      it "create a collection, invalid name" do
        cmd = api
        body = "{ \"name\" : \"_invalid\" }"
        doc = ArangoDB.log_post("#{prefix}-create-collection-invalid", cmd, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
      end
      
      it "create a collection, already existing" do
        ArangoDB.drop_collection(@cn)
        cmd = api
        body = "{ \"name\" : \"#{@cn}\" }"
        doc = ArangoDB.log_post("#{prefix}-create-collection-existing", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
              
        body = "{ \"name\" : \"#{@cn}\" }"
        doc = ArangoDB.log_post("#{prefix}-create-collection-existing", cmd, :body => body)

        doc.code.should eq(409)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(409)
  
        ArangoDB.drop_collection(@cn)
      end
    end

################################################################################
## load a collection
################################################################################

    context "loading:" do
      before do
        @cn = "UnitTestsCollectionBasics"
      end

      it "load a collection by identifier" do
        ArangoDB.drop_collection(@cn)
        cid = ArangoDB.create_collection(@cn)

        cmd = api + "/" + @cn + "/load"
        doc = ArangoDB.log_put("#{prefix}-identifier-load", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(cid)
        doc.parsed_response['name'].should eq(@cn)
        doc.parsed_response['status'].should eq(3)
        doc.parsed_response['count'].should be_kind_of(Integer)
        doc.parsed_response['count'].should eq(0)

        ArangoDB.drop_collection(@cn)
      end

      it "load a collection by name" do
        ArangoDB.drop_collection(@cn)
        cid = ArangoDB.create_collection(@cn)

        cmd = api + "/" + @cn + "/load"
        doc = ArangoDB.log_put("#{prefix}-name-load", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(cid)
        doc.parsed_response['name'].should eq(@cn)
        doc.parsed_response['status'].should eq(3)
        doc.parsed_response['count'].should be_kind_of(Integer)
        doc.parsed_response['count'].should eq(0)

        ArangoDB.drop_collection(@cn)
      end

      it "load a collection by name with explicit count" do
        ArangoDB.drop_collection(@cn)
        cid = ArangoDB.create_collection(@cn)

        cmd = "/_api/document?collection=#{@cn}"
        body = "{ \"Hallo\" : \"World\" }"

        for i in ( 1 .. 10 )
          doc = ArangoDB.post(cmd, :body => body)
        end

        cmd = api + "/" + @cn + "/load"
        body = "{ \"count\" : true }"
        doc = ArangoDB.log_put("#{prefix}-name-load", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(cid)
        doc.parsed_response['name'].should eq(@cn)
        doc.parsed_response['status'].should eq(3)
        doc.parsed_response['count'].should be_kind_of(Integer)
        doc.parsed_response['count'].should eq(10)

        ArangoDB.drop_collection(@cn)
      end

      it "load a collection by name without count" do
        ArangoDB.drop_collection(@cn)
        cid = ArangoDB.create_collection(@cn)
        
        cmd = "/_api/document?collection=#{@cn}"
        body = "{ \"Hallo\" : \"World\" }"
        doc = ArangoDB.post(cmd, :body => body)

        cmd = api + "/" + @cn + "/load"
        body = "{ \"count\" : false }"
        doc = ArangoDB.log_put("#{prefix}-name-load", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(cid)
        doc.parsed_response['name'].should eq(@cn)
        doc.parsed_response['status'].should eq(3)
        doc.parsed_response['count'].should be_nil

        ArangoDB.drop_collection(@cn)
      end
    end

################################################################################
## unloading a collection
################################################################################

    context "unloading:" do
      before do
        @cn = "UnitTestsCollectionBasics"
      end

      it "unload a collection by identifier" do
        ArangoDB.drop_collection(@cn)
        cid = ArangoDB.create_collection(@cn)

        cmd = api + "/" + @cn + "/unload"
        doc = ArangoDB.log_put("#{prefix}-identifier-unload", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(cid)
        doc.parsed_response['name'].should eq(@cn)
        [2, 4].should include(doc.parsed_response['status'])

        ArangoDB.drop_collection(@cn)
      end

      it "unload a collection by name" do
        ArangoDB.drop_collection(@cn)
        cid = ArangoDB.create_collection(@cn)

        cmd = api + "/" + @cn + "/unload"
        doc = ArangoDB.log_put("#{prefix}-name-unload", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(cid)
        doc.parsed_response['name'].should eq(@cn)
        [2, 4].should include(doc.parsed_response['status'])

        ArangoDB.drop_collection(@cn)
      end
    end

################################################################################
## truncate a collection
################################################################################

    context "truncating:" do
      before do
        @cn = "UnitTestsCollectionBasics"
        @cid = ArangoDB.create_collection(@cn)
      end

      after do
        ArangoDB.drop_collection(@cn)
      end

      it "truncate a collection by identifier" do
        cmd = "/_api/document?collection=#{@cid}"
        body = "{ \"Hallo\" : \"World\" }"

        for i in ( 1 .. 10 )
          doc = ArangoDB.post(cmd, :body => body)
        end

        ArangoDB.size_collection(@cid).to_i.should eq(10)

        cmd = api + "/" + @cn + "/truncate"
        doc = ArangoDB.log_put("#{prefix}-identifier-truncate", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(@cid)
        doc.parsed_response['name'].should eq(@cn)
        doc.parsed_response['status'].should eq(3)

        ArangoDB.size_collection(@cid).should eq(0)

        ArangoDB.drop_collection(@cn)
      end
    end

################################################################################
## properties of a collection
################################################################################

    context "properties:" do
      it "changing the journalSize to null" do
        cn = "UnitTestsCollectionBasics"
        ArangoDB.drop_collection(cn)
        cid = ArangoDB.create_collection(cn)

        cmd = api + "/" + cn + "/properties"
        body = "{ \"journalSize\" : 1048576 }"
        doc = ArangoDB.log_put("#{prefix}-properties-journalSize", cmd, :body => body)
        doc.code.should eq(200)

        doc = ArangoDB.log_get("#{prefix}-properties-journalSize", cmd)
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['journalSize'].should eq(1048576)
        
        body = "{ \"journalSize\" : 8388608 }"
        doc = ArangoDB.log_put("#{prefix}-properties-journalSize", cmd, :body => body)
        doc.code.should eq(200)

        doc = ArangoDB.log_get("#{prefix}-properties-journalSize", cmd)
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['journalSize'].should eq(8388608)
        
        body = "{ \"journalSize\" : null }"
        doc = ArangoDB.log_put("#{prefix}-properties-journalSize", cmd, :body => body)
        doc.code.should eq(200)

        doc = ArangoDB.log_get("#{prefix}-properties-journalSize", cmd)
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['journalSize'].should eq(8388608)
        
        body = "{ }"
        doc = ArangoDB.log_put("#{prefix}-properties-journalSize", cmd, :body => body)
        doc.code.should eq(200)

        doc = ArangoDB.log_get("#{prefix}-properties-journalSize", cmd)
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['journalSize'].should eq(8388608)

        ArangoDB.drop_collection(cn)
      end

      it "changing the properties of a collection by identifier" do
        cn = "UnitTestsCollectionBasics"
        ArangoDB.drop_collection(cn)
        cid = ArangoDB.create_collection(cn)

        cmd = "/_api/document?collection=#{cid}"
        body = "{ \"Hallo\" : \"World\" }"

        for i in ( 1 .. 10 )
          doc = ArangoDB.post(cmd, :body => body)
        end

        ArangoDB.size_collection(cid).should eq(10)
        ArangoDB.size_collection(cn).should eq(10)

        cmd = api + "/" + cn + "/properties"
        body = "{ \"waitForSync\" : true }"
        doc = ArangoDB.log_put("#{prefix}-identifier-properties-sync", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(cid)
        doc.parsed_response['name'].should eq(cn)
        doc.parsed_response['status'].should eq(3)
        doc.parsed_response['doCompact'].should eq(true)
        doc.parsed_response['waitForSync'].should eq(true)
        doc.parsed_response['isVolatile'].should eq(false)
        doc.parsed_response['isSystem'].should eq(false)
        doc.parsed_response['keyOptions']['type'].should eq("traditional")
        doc.parsed_response['keyOptions']['allowUserKeys'].should eq(true)

        cmd = api + "/" + cn + "/properties"
        body = "{ \"waitForSync\" : false }"
        doc = ArangoDB.log_put("#{prefix}-identifier-properties-no-sync", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(cid)
        doc.parsed_response['name'].should eq(cn)
        doc.parsed_response['status'].should eq(3)
        doc.parsed_response['doCompact'].should eq(true)
        doc.parsed_response['waitForSync'].should eq(false)
        doc.parsed_response['isVolatile'].should eq(false)
        doc.parsed_response['isSystem'].should eq(false)
        doc.parsed_response['keyOptions']['type'].should eq("traditional")
        doc.parsed_response['keyOptions']['allowUserKeys'].should eq(true)
        
        body = "{ \"doCompact\" : false }"
        doc = ArangoDB.log_put("#{prefix}-identifier-properties-no-compact", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(cid)
        doc.parsed_response['name'].should eq(cn)
        doc.parsed_response['status'].should eq(3)
        doc.parsed_response['doCompact'].should eq(false)
        doc.parsed_response['waitForSync'].should eq(false)
        doc.parsed_response['isVolatile'].should eq(false)
        doc.parsed_response['isSystem'].should eq(false)
        doc.parsed_response['keyOptions']['type'].should eq("traditional")
        doc.parsed_response['keyOptions']['allowUserKeys'].should eq(true)

        ArangoDB.drop_collection(cn)
      end

      it "create collection with explicit keyOptions property, traditional keygen" do
        cn = "UnitTestsCollectionBasics"

        cmd = "/_api/collection"
        body = "{ \"name\" : \"#{cn}\", \"waitForSync\" : false, \"type\" : 2, \"keyOptions\" : {\"type\": \"traditional\", \"allowUserKeys\": true } }"
        doc = ArangoDB.log_post("#{prefix}-with-create-options", cmd, :body => body)

        doc.code.should eq(200)
        cid = doc.parsed_response['id']

        cmd = api + "/" + cn + "/properties"
        body = "{ \"waitForSync\" : true }"
        doc = ArangoDB.log_put("#{prefix}-with-create-options", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(cid)
        doc.parsed_response['name'].should eq(cn)
        doc.parsed_response['status'].should eq(3)
        doc.parsed_response['waitForSync'].should eq(true)
        doc.parsed_response['isVolatile'].should eq(false)
        doc.parsed_response['keyOptions']['type'].should eq("traditional")
        doc.parsed_response['keyOptions']['allowUserKeys'].should eq(true)

        ArangoDB.drop_collection(cn)
      end

      it "create a collection with isVolatile property" do
        cn = "UnitTestsCollectionBasics"
        ArangoDB.drop_collection(cn)

        cmd = "/_api/collection"
        body = "{ \"name\" : \"#{cn}\", \"isVolatile\" : true }"
        doc = ArangoDB.log_post("#{prefix}-with-volatile", cmd, :body => body)

        doc.code.should eq(200)
        cid = doc.parsed_response['id']

        cmd = api + "/" + cn + "/properties"
        doc = ArangoDB.log_get("#{prefix}-with-volatile", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['doCompact'].should eq(true)
        doc.parsed_response['waitForSync'].should eq(false)
        doc.parsed_response['isVolatile'].should eq(true)
        doc.parsed_response['keyOptions']['type'].should eq("traditional")
        doc.parsed_response['keyOptions']['allowUserKeys'].should eq(true)

        ArangoDB.drop_collection(cn)
      end
      
      it "create collection with empty keyOptions property" do
        cn = "UnitTestsCollectionBasics"
        ArangoDB.drop_collection(cn)

        cmd = "/_api/collection"
        body = "{ \"name\" : \"#{cn}\", \"waitForSync\" : false, \"type\" : 2 }"
        doc = ArangoDB.log_post("#{prefix}-with-empty-create-options", cmd, :body => body)

        doc.code.should eq(200)
        cid = doc.parsed_response['id']

        cmd = api + "/" + cn + "/properties"
        body = "{ \"waitForSync\" : true }"
        doc = ArangoDB.log_put("#{prefix}-with-empty-create-options", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(cid)
        doc.parsed_response['name'].should eq(cn)
        doc.parsed_response['status'].should eq(3)
        doc.parsed_response['doCompact'].should eq(true)
        doc.parsed_response['waitForSync'].should eq(true)
        doc.parsed_response['isVolatile'].should eq(false)
        doc.parsed_response['keyOptions']['type'].should eq("traditional")
        doc.parsed_response['keyOptions']['allowUserKeys'].should eq(true)

        ArangoDB.drop_collection(cn)
      end

    end

  end
end
