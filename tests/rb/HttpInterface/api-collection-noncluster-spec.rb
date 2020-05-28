# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_api/collection"
  prefix = "api-collection"

  context "dealing with collections:" do
    before do
      @cn = "UnitTestsCollectionBasics"
      ArangoDB.drop_collection(@cn)
      @cid = ArangoDB.create_collection(@cn)
    end

    after do
      ArangoDB.drop_collection(@cn)
    end

################################################################################
## checksum
################################################################################

    context "checksum:" do
      # checksum
      it "calculating the checksum for a collection" do
        cmd = api + "/" + @cn + "/checksum"
        doc = ArangoDB.log_get("#{prefix}-get-collection-checksum", cmd)

        # empty collection
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
        c1 = doc.parsed_response['checksum']
        c1.should be_kind_of(String)
        c1.should eq("0");

        # create a new document
        body = "{ \"test\" : 1 }"
        doc = ArangoDB.log_post("#{prefix}-get-collection-checksum", "/_api/document/?collection=" + @cn, :body => body)

        # fetch checksum again
        doc = ArangoDB.log_get("#{prefix}-get-collection-checksum", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)

        r2 = doc.parsed_response['revision']
        r2.should_not eq("");
        r2.should_not eq(r1);
        c2 = doc.parsed_response['checksum']
        c2.should be_kind_of(String)
        c2.should_not eq("0");
        c2.should_not eq(c1);

        # create another document
        doc = ArangoDB.log_post("#{prefix}-get-collection-checksum", "/_api/document/?collection=" + @cn, :body => body)

        # fetch checksum again
        doc = ArangoDB.log_get("#{prefix}-get-collection-checksum", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)

        r3 = doc.parsed_response['revision']
        r3.should_not eq("");
        r3.should_not eq(r1);
        r3.should_not eq(r2);
        c3 = doc.parsed_response['checksum']
        c3.should be_kind_of(String)
        c3.should_not eq("0");
        c3.should_not eq(c1);
        c3.should_not eq(c2);

        # fetch checksum <withData>
        doc = ArangoDB.log_get("#{prefix}-get-collection-checksum", cmd + "?withData=true")

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)

        r4 = doc.parsed_response['revision']
        r4.should eq(r3);
        c4 = doc.parsed_response['checksum']
        c4.should be_kind_of(String)
        c4.should_not eq("0");
        c4.should_not eq(c1);
        c4.should_not eq(c2);
        c4.should_not eq(c3);

        # truncate
        doc = ArangoDB.log_put("#{prefix}-get-collection-checksum", "/_api/collection/#{@cn}/truncate", :body => "")

        # fetch checksum again
        doc = ArangoDB.log_get("#{prefix}-get-collection-checksum", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)

        r5 = doc.parsed_response['revision']
        r5.should_not eq("");
        c5 = doc.parsed_response['checksum']
        c5.should be_kind_of(String)
        c5.should eq("0");
      end
    end

################################################################################
## properties of a collection
################################################################################

    context "properties:" do
      it "create collection with explicit keyOptions property, autoinc keygen" do
        cn = "UnitTestsCollectionKeygen"

        ArangoDB.drop_collection(cn)

        cmd = "/_api/collection"
        body = "{ \"name\" : \"#{cn}\", \"waitForSync\" : false, \"type\" : 2, \"keyOptions\" : {\"type\": \"autoincrement\", \"offset\": 7, \"increment\": 99, \"allowUserKeys\": false } }"
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
        doc.parsed_response['objectId'].should_not eq(0)
        doc.parsed_response['keyOptions']['type'].should eq("autoincrement")
        doc.parsed_response['keyOptions']['increment'].should eq(99)
        doc.parsed_response['keyOptions']['offset'].should eq(7)
        doc.parsed_response['keyOptions']['allowUserKeys'].should eq(false)

        ArangoDB.drop_collection(cn)
      end

    end

################################################################################
## renames a collection
################################################################################

    context "renaming:" do
      it "rename a collection by identifier" do
        cmd = "/_api/document?collection=#{@cn}"
        body = "{ \"Hallo\" : \"World\" }"

        for i in ( 1 .. 10 )
          doc = ArangoDB.post(cmd, :body => body)
        end

        ArangoDB.size_collection(@cid).should eq(10)
        ArangoDB.size_collection(@cn).should eq(10)

        cn2 = "UnitTestsCollectionBasics2"
        ArangoDB.drop_collection(cn2)

        body = "{ \"name\" : \"#{cn2}\" }"
        cmd = api + "/" + @cn + "/rename"
        doc = ArangoDB.log_put("#{prefix}-identifier-rename", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(@cid)
        doc.parsed_response['name'].should eq(cn2)
        doc.parsed_response['status'].should eq(3)

        ArangoDB.size_collection(@cid).should eq(10)
        ArangoDB.size_collection(cn2).should eq(10)

        cmd = api + "/" + @cn
        doc = ArangoDB.get(cmd)

        doc.code.should eq(404)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1203)

        cmd = api + "/" + cn2
        doc = ArangoDB.get(cmd)

        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['name'].should eq(cn2)
        doc.parsed_response['status'].should eq(3)

        ArangoDB.drop_collection(@cn)
        ArangoDB.drop_collection(cn2)
      end

      it "rename a collection by identifier with conflict" do
        cn2 = "UnitTestsCollectionBasics2"
        ArangoDB.drop_collection(cn2)
        cid2 = ArangoDB.create_collection(cn2)

        body = "{ \"Hallo\" : \"World\" }"

        cmd = "/_api/document?collection=#{@cn}"

        for i in ( 1 .. 10 )
          doc = ArangoDB.post(cmd, :body => body)
        end

        ArangoDB.size_collection(@cid).should eq(10)
        ArangoDB.size_collection(@cn).should eq(10)

        cmd = "/_api/document?collection=#{cn2}"

        for i in ( 1 .. 20 )
          doc = ArangoDB.post(cmd, :body => body)
        end

        ArangoDB.size_collection(cid2).should eq(20)
        ArangoDB.size_collection(cn2).should eq(20)

        body = "{ \"name\" : \"#{cn2}\" }"
        cmd = api + "/" + @cn + "/rename"
        doc = ArangoDB.log_put("#{prefix}-identifier-rename-conflict", cmd, :body => body)

        doc.code.should eq(409)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(409)
        doc.parsed_response['errorNum'].should eq(1207)

        ArangoDB.size_collection(@cid).should eq(10)
        ArangoDB.size_collection(@cn).should eq(10)

        ArangoDB.size_collection(cid2).should eq(20)
        ArangoDB.size_collection(cn2).should eq(20)

        ArangoDB.drop_collection(@cn)
        ArangoDB.drop_collection(cn2)
      end

      it "rename a new-born collection by identifier" do
        cn2 = "UnitTestsCollectionBasics2"
        ArangoDB.drop_collection(cn2)

        body = "{ \"name\" : \"#{cn2}\" }"
        cmd = api + "/" + @cn + "/rename"
        doc = ArangoDB.log_put("#{prefix}-identifier-rename-new-born", cmd, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['id'].should eq(@cid)
        doc.parsed_response['name'].should eq(cn2)
        doc.parsed_response['status'].should eq(3)

        cmd = api + "/" + @cn
        doc = ArangoDB.get(cmd)

        doc.code.should eq(404)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['errorNum'].should eq(1203)

        cmd = api + "/" + cn2
        doc = ArangoDB.get(cmd)

        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['name'].should eq(cn2)
        doc.parsed_response['status'].should eq(3)

        ArangoDB.drop_collection(@cn)
        ArangoDB.drop_collection(cn2)
      end

      it "rename a new-born collection by identifier with conflict" do
        cn2 = "UnitTestsCollectionBasics2"
        ArangoDB.drop_collection(cn2)
        cid2 = ArangoDB.create_collection(cn2)

        cmd = "/_api/document?collection=#{cn2}"

        body = "{ \"name\" : \"#{cn2}\" }"
        cmd = api + "/" + @cn + "/rename"
        doc = ArangoDB.log_put("#{prefix}-identifier-rename-conflict", cmd, :body => body)

        doc.code.should eq(409)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(409)
        doc.parsed_response['errorNum'].should eq(1207)

        ArangoDB.drop_collection(@cn)
        ArangoDB.drop_collection(cn2)
      end
    end


  end
end
