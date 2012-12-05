# coding: utf-8

require 'rspec'
require './arangodb.rb'


describe ArangoDB do
  prefix = "api-blueprints"

  vertex_collection = "v"
  edge_collection = "e"
  graph_name = "graph1"

  context "testing blueprints methods:" do

  def truncate_collection (prefix, name)
    cmd = "/_api/collection/#{name}/truncate"
    ArangoDB.log_put("#{prefix}", cmd)
  end

################################################################################
## checking graph responses
################################################################################

    context "checks blueprints graph requests" do
      before do
        #ArangoDB.create_collection( edge_collection , 0, 3)
        #ArangoDB.create_collection( vertex_collection , 0, 2)
        truncate_collection(prefix, "_graphs")
      end

      after do
        truncate_collection(prefix, "_graphs")
        ArangoDB.drop_collection( vertex_collection )
        ArangoDB.drop_collection( edge_collection )
      end
      
      it "checks create graph" do
        cmd = "/_api/blueprints/graph"
      	body = "{\"name\" : \"#{graph_name}\", \"verticesName\" : \"#{vertex_collection}\", \"edgesName\" : \"#{edge_collection}\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

      	doc.code.should eq(200)
      	doc.parsed_response['error'].should eq(false)
	      doc.parsed_response['code'].should eq(200)
        doc.parsed_response['graph']['name'].should eq("#{graph_name}")
      end

      it "checks create graph with wrong edges collection" do
        ArangoDB.create_collection( edge_collection , 0, 2)

        cmd = "/_api/blueprints/graph"
      	body = "{\"name\" : \"#{graph_name}\", \"verticesName\" : \"#{vertex_collection}\", \"edgesName\" : \"#{edge_collection}\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

      	doc.code.should eq(400)
      	doc.parsed_response['error'].should eq(true)
	      doc.parsed_response['code'].should eq(400)
	      doc.parsed_response['errorNum'].should eq(1902) 
      end

      it "checks (re)create graph" do
        cmd = "/_api/blueprints/graph"
      	body = "{\"name\" : \"#{graph_name}\", \"verticesName\" : \"#{vertex_collection}\", \"edgesName\" : \"#{edge_collection}\"}"
        doc1 = ArangoDB.log_post("#{prefix}", cmd, :body => body)

      	doc1.code.should eq(200)

        doc2 = ArangoDB.log_post("#{prefix}", cmd, :body => body)

      	doc2.code.should eq(200)

        doc1.parsed_response['graph']['_id'].should eq(doc2.parsed_response['graph']['_id'])
      end

      it "checks create and get graph" do
        cmd = "/_api/blueprints/graph"
      	body = "{\"name\" : \"#{graph_name}\", \"verticesName\" : \"#{vertex_collection}\", \"edgesName\" : \"#{edge_collection}\"}"
        doc1 = ArangoDB.log_post("#{prefix}", cmd, :body => body)
        g_id = doc1.parsed_response['graph']['_id']

      	doc1.code.should eq(200)

        cmd = "/_api/blueprints/graph/#{graph_name}"
        doc2 = ArangoDB.log_get("#{prefix}", cmd)

      	doc2.code.should eq(200)
        doc2.parsed_response['graph']['_id'].should eq(g_id)

        cmd = "/_api/blueprints/graph/#{g_id}"
        doc3 = ArangoDB.log_get("#{prefix}", cmd)

      	doc3.code.should eq(200)
        doc3.parsed_response['graph']['_id'].should eq(g_id)
      end

      it "checks create and delete graph" do
        # create
        cmd = "/_api/blueprints/graph"
      	body = "{\"name\" : \"#{graph_name}\", \"verticesName\" : \"#{vertex_collection}\", \"edgesName\" : \"#{edge_collection}\"}"
        doc1 = ArangoDB.log_post("#{prefix}", cmd, :body => body)
      	doc1.code.should eq(200)

        # delete
        cmd = "/_api/blueprints/graph/#{graph_name}"
        doc2 = ArangoDB.log_delete("#{prefix}", cmd)
      	doc2.code.should eq(200)
        doc2.parsed_response['deleted'].should eq(true)

        # check
        doc3 = ArangoDB.log_get("#{prefix}", cmd)
      	doc3.code.should eq(400)
      end

    end

################################################################################
## checking vertex responses
################################################################################

    context "checks blueprints vertex requests" do
      before do
        truncate_collection(prefix, "_graphs")
        cmd = "/_api/blueprints/graph"
      	body = "{\"name\" : \"#{graph_name}\", \"verticesName\" : \"#{vertex_collection}\", \"edgesName\" : \"#{edge_collection}\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
      end

      after do
        truncate_collection(prefix, "_graphs")
        ArangoDB.drop_collection( vertex_collection )
        ArangoDB.drop_collection( edge_collection )
      end
      
      it "checks create vertex with \$id" do
        cmd = "/_api/blueprints/vertex?graph=#{graph_name}"
      	body = "{\"\$id\" : \"v1\", \"optional1\" : \"val1\", \"optional2\" : \"val2\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

      	doc.code.should eq(200)
      	doc.parsed_response['error'].should eq(false)
	      doc.parsed_response['code'].should eq(200)
        doc.parsed_response['vertex']['$id'].should eq("v1")
        doc.parsed_response['vertex']['optional1'].should eq("val1")
      end

      it "checks create second vertex with \$id" do
        cmd = "/_api/blueprints/vertex?graph=#{graph_name}"
      	body = "{\"\$id\" : \"double_id\", \"optional1\" : \"val1\", \"optional2\" : \"val2\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

      	doc.code.should eq(200)
      	doc.parsed_response['error'].should eq(false)
	      doc.parsed_response['code'].should eq(200)

        doc2 = ArangoDB.log_post("#{prefix}", cmd, :body => body)
      	doc2.code.should eq(400)
      	doc2.parsed_response['error'].should eq(true)
	      doc2.parsed_response['code'].should eq(400)
      end

      it "checks get vertex by \$id" do
        cmd = "/_api/blueprints/vertex?graph=#{graph_name}"
      	body = "{\"\$id\" : \"v1\", \"optional1\" : \"val1\", \"optional2\" : \"val2\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

      	doc.code.should eq(200)
      	doc.parsed_response['error'].should eq(false)
	      doc.parsed_response['code'].should eq(200)
        doc.parsed_response['vertex']['$id'].should eq("v1")
        doc.parsed_response['vertex']['optional1'].should eq("val1")

        cmd = "/_api/blueprints/vertex/v1?graph=#{graph_name}"
        doc2 = ArangoDB.log_get("#{prefix}", cmd)
      	doc2.code.should eq(200)
      	doc2.parsed_response['error'].should eq(false)
	      doc2.parsed_response['code'].should eq(200)
        doc2.parsed_response['vertex']['optional1'].should eq(doc.parsed_response['vertex']['optional1'])

        _id = doc2.parsed_response['vertex']['_id'];
        cmd = "/_api/blueprints/vertex/#{_id}?graph=#{graph_name}"
        doc3 = ArangoDB.log_get("#{prefix}", cmd)
      	doc3.code.should eq(200)
      	doc3.parsed_response['error'].should eq(false)
	      doc3.parsed_response['code'].should eq(200)
        doc3.parsed_response['vertex']['optional1'].should eq(doc.parsed_response['vertex']['optional1'])
      end

      it "checks get vertex by wrong \$id" do
        cmd = "/_api/blueprints/vertex/vv11?graph=#{graph_name}"
        doc = ArangoDB.log_get("#{prefix}", cmd)
      	doc.code.should eq(400)
      	doc.parsed_response['error'].should eq(true)
	      doc.parsed_response['code'].should eq(400)
      end

      it "checks update vertex by \$id" do
        cmd = "/_api/blueprints/vertex?graph=#{graph_name}"
      	body = "{\"\$id\" : \"v1\", \"optional1\" : \"val1\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

      	doc.code.should eq(200)
      	doc.parsed_response['error'].should eq(false)
	      doc.parsed_response['code'].should eq(200)
        doc.parsed_response['vertex']['$id'].should eq("v1")
        doc.parsed_response['vertex']['optional1'].should eq("val1")

        cmd = "/_api/blueprints/vertex/v1?graph=#{graph_name}"
        doc2 = ArangoDB.log_get("#{prefix}", cmd)
      	doc2.code.should eq(200)
      	doc2.parsed_response['error'].should eq(false)
	      doc2.parsed_response['code'].should eq(200)
        doc2.parsed_response['vertex']['optional1'].should eq(doc.parsed_response['vertex']['optional1'])

        _id = doc2.parsed_response['vertex']['_id'];
        cmd = "/_api/blueprints/vertex/#{_id}?graph=#{graph_name}"
      	body = "{\"\$id\" : \"v1\", \"optional1\" : \"val2\"}"
        doc3 = ArangoDB.log_put("#{prefix}", cmd, :body => body)
      	doc3.code.should eq(200)
      	doc3.parsed_response['error'].should eq(false)
	      doc3.parsed_response['code'].should eq(200)
        doc3.parsed_response['vertex']['optional1'].should eq("val2")

        cmd = "/_api/blueprints/vertex/v1?graph=#{graph_name}"
        doc2 = ArangoDB.log_get("#{prefix}", cmd)
      	doc2.code.should eq(200)
      	doc2.parsed_response['error'].should eq(false)
	      doc2.parsed_response['code'].should eq(200)
        doc2.parsed_response['vertex']['optional1'].should eq("val2")
      end

      it "checks update vertex" do
        cmd = "/_api/blueprints/vertex?graph=#{graph_name}"
      	body = "{\"optional1\" : \"val1\", \"optional2\" : \"val2\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

      	doc.code.should eq(200)
      	doc.parsed_response['error'].should eq(false)
	      doc.parsed_response['code'].should eq(200)
        doc.parsed_response['vertex']['$id'].should eq(nil)
        doc.parsed_response['vertex']['optional1'].should eq("val1")
        doc.parsed_response['vertex']['optional2'].should eq("val2")
        _id = doc.parsed_response['vertex']['_id'];

        cmd = "/_api/blueprints/vertex/#{_id}?graph=#{graph_name}"
        doc2 = ArangoDB.log_get("#{prefix}", cmd)
      	doc2.code.should eq(200)
      	doc2.parsed_response['error'].should eq(false)
	      doc2.parsed_response['code'].should eq(200)
        doc2.parsed_response['vertex']['optional1'].should eq(doc.parsed_response['vertex']['optional1'])

        cmd = "/_api/blueprints/vertex/#{_id}?graph=#{graph_name}"
      	body = "{\"optional1\" : \"val2\"}"
        doc3 = ArangoDB.log_put("#{prefix}", cmd, :body => body)
      	doc3.code.should eq(200)
      	doc3.parsed_response['error'].should eq(false)
	      doc3.parsed_response['code'].should eq(200)
        doc3.parsed_response['vertex']['optional1'].should eq("val2")
        doc3.parsed_response['vertex']['optional2'].should eq(nil)

        cmd = "/_api/blueprints/vertex/#{_id}?graph=#{graph_name}"
        doc4 = ArangoDB.log_get("#{prefix}", cmd)
      	doc4.code.should eq(200)
      	doc4.parsed_response['error'].should eq(false)
	      doc4.parsed_response['code'].should eq(200)
        doc4.parsed_response['vertex']['optional1'].should eq("val2")
        doc4.parsed_response['vertex']['optional2'].should eq(nil)
      end

      it "checks delete vertex" do
        cmd = "/_api/blueprints/vertex?graph=#{graph_name}"
      	body = "{\"optional1\" : \"val1\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

      	doc.code.should eq(200)
      	doc.parsed_response['error'].should eq(false)
	      doc.parsed_response['code'].should eq(200)
        doc.parsed_response['vertex']['$id'].should eq(nil)
        doc.parsed_response['vertex']['optional1'].should eq("val1")
        _id = doc.parsed_response['vertex']['_id'];

        cmd = "/_api/blueprints/vertex/#{_id}?graph=#{graph_name}"
        doc2 = ArangoDB.log_delete("#{prefix}", cmd)
      	doc2.code.should eq(200)
      	doc2.parsed_response['error'].should eq(false)
	      doc2.parsed_response['code'].should eq(200)
        doc2.parsed_response['deleted'].should eq(true)

        cmd = "/_api/blueprints/vertex/#{_id}?graph=#{graph_name}"
        doc3 = ArangoDB.log_get("#{prefix}", cmd)
      	doc3.code.should eq(400)
      	doc3.parsed_response['error'].should eq(true)
	      doc3.parsed_response['code'].should eq(400)
      end

    end

################################################################################
## checking vertices responses
################################################################################

    context "checks blueprints vertices requests" do
      before do
        truncate_collection(prefix, "_graphs")
        cmd = "/_api/blueprints/graph"
      	body = "{\"name\" : \"#{graph_name}\", \"verticesName\" : \"#{vertex_collection}\", \"edgesName\" : \"#{edge_collection}\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
      end

      after do
        truncate_collection(prefix, "_graphs")
        ArangoDB.drop_collection( vertex_collection )
        ArangoDB.drop_collection( edge_collection )
      end
      
      it "checks list of vertices" do
        cmd = "/_api/blueprints/vertex?graph=#{graph_name}"
      	body = "{\"$id\" : \"id1\", \"optional1\" : \"val1\", \"optional2\" : 1}"
        ArangoDB.log_post("#{prefix}", cmd, :body => body)
      	body = "{\"$id\" : \"id2\", \"optional1\" : \"val1\", \"optional2\" : 2}"
        ArangoDB.log_post("#{prefix}", cmd, :body => body)
      	body = "{\"$id\" : \"id3\", \"optional1\" : \"val1\", \"optional2\" : 2}"
        ArangoDB.log_post("#{prefix}", cmd, :body => body)
      	body = "{\"$id\" : \"id4\", \"optional1\" : \"val1\", \"optional2\" : 3}"
        ArangoDB.log_post("#{prefix}", cmd, :body => body)
      	body = "{\"$id\" : \"id5\", \"optional2\" : \"val2\"}"
        ArangoDB.log_post("#{prefix}", cmd, :body => body)

        cmd = "/_api/blueprints/edge?graph=#{graph_name}"
      	body = "{\"$id\" : \"edge1\", \"_from\" : \"id1\", \"_to\" : \"id2\", \"$label\" : \"l1\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
      	body = "{\"$id\" : \"edge2\", \"_from\" : \"id2\", \"_to\" : \"id3\", \"$label\" : \"l2\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

        cmd = "/_api/blueprints/vertices?graph=#{graph_name}"
      	body = "{\"batchSize\" : 100 }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
      	doc.code.should eq(201)
      	doc.parsed_response['error'].should eq(false)
	      doc.parsed_response['code'].should eq(201)
	      doc.parsed_response['result'].count.should eq(5)

        cmd = "/_api/blueprints/vertices?graph=#{graph_name}"
      	body = "{\"batchSize\" : 100, \"key\" : \"optional2\", \"value\" : 3 }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
      	doc.code.should eq(201)
      	doc.parsed_response['error'].should eq(false)
	      doc.parsed_response['code'].should eq(201)
	      doc.parsed_response['result'].count.should eq(1)

        cmd = "/_api/blueprints/vertices?graph=#{graph_name}"
      	body = "{\"batchSize\" : 100, \"key\" : \"optional2\", \"value\" : 2 }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
      	doc.code.should eq(201)
      	doc.parsed_response['error'].should eq(false)
	      doc.parsed_response['code'].should eq(201)
	      doc.parsed_response['result'].count.should eq(2)

        cmd = "/_api/blueprints/vertices?graph=#{graph_name}"
      	body = "{\"batchSize\" : 100, \"vertex\" : \"id2\" }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
      	doc.code.should eq(201)
      	doc.parsed_response['error'].should eq(false)
	      doc.parsed_response['code'].should eq(201)
	      doc.parsed_response['result'].count.should eq(2)

        cmd = "/_api/blueprints/vertices?graph=#{graph_name}"
      	body = "{\"batchSize\" : 100, \"vertex\" : \"id2\", \"direction\" : \"in\" }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
      	doc.code.should eq(201)
      	doc.parsed_response['error'].should eq(false)
	      doc.parsed_response['code'].should eq(201)
	      doc.parsed_response['result'].count.should eq(1)
	      doc.parsed_response['result'][0]['$id'].should eq("id1")

        cmd = "/_api/blueprints/vertices?graph=#{graph_name}"
      	body = "{\"batchSize\" : 100, \"vertex\" : \"id2\", \"direction\" : \"out\" }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
      	doc.code.should eq(201)
      	doc.parsed_response['error'].should eq(false)
	      doc.parsed_response['code'].should eq(201)
	      doc.parsed_response['result'].count.should eq(1)
	      doc.parsed_response['result'][0]['$id'].should eq("id3")

        cmd = "/_api/blueprints/vertices?graph=#{graph_name}"
      	body = "{\"batchSize\" : 100, \"vertex\" : \"id2\", \"labels\" : [\"l2\"] }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
      	doc.code.should eq(201)
      	doc.parsed_response['error'].should eq(false)
	      doc.parsed_response['code'].should eq(201)
	      doc.parsed_response['result'].count.should eq(1)
	      doc.parsed_response['result'][0]['$id'].should eq("id3")

      end

    end

################################################################################
## checking edge responses
################################################################################

    context "checks blueprints edge requests" do
      before do
        truncate_collection(prefix, "_graphs")
        cmd = "/_api/blueprints/graph"
      	body = "{\"name\" : \"#{graph_name}\", \"verticesName\" : \"#{vertex_collection}\", \"edgesName\" : \"#{edge_collection}\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

        cmd = "/_api/blueprints/vertex?graph=#{graph_name}"
      	body = "{\"$id\" : \"vert1\"}"
        ArangoDB.log_post("#{prefix}", cmd, :body => body)
      	body = "{\"$id\" : \"vert2\"}"
        ArangoDB.log_post("#{prefix}", cmd, :body => body)
      	body = "{\"$id\" : \"vert3\"}"
        ArangoDB.log_post("#{prefix}", cmd, :body => body)

      end

      after do
        truncate_collection(prefix, "_graphs")
        ArangoDB.drop_collection( vertex_collection )
        ArangoDB.drop_collection( edge_collection )
      end
      
      it "checks create edge" do
        cmd = "/_api/blueprints/edge?graph=#{graph_name}"
      	body = "{\"$id\" : \"edge1\", \"_from\" : \"vert2\", \"_to\" : \"vert1\", \"optional1\" : \"val1\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

      	doc.code.should eq(200)
      	doc.parsed_response['error'].should eq(false)
	      doc.parsed_response['code'].should eq(200)
	      doc.parsed_response['edge']['$id'].should eq("edge1")
	      doc.parsed_response['edge']['optional1'].should eq("val1")
	      doc.parsed_response['edge']['$label'].should eq(nil)
      end

      it "checks create second edge with same \$id" do
        cmd = "/_api/blueprints/edge?graph=#{graph_name}"
      	body = "{\"$id\" : \"edge1\", \"_from\" : \"vert2\", \"_to\" : \"vert1\", \"optional1\" : \"val1\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

      	doc.code.should eq(200)
      	doc.parsed_response['error'].should eq(false)
	      doc.parsed_response['code'].should eq(200)
	      doc.parsed_response['edge']['$id'].should eq("edge1")
	      doc.parsed_response['edge']['optional1'].should eq("val1")
	      doc.parsed_response['edge']['$label'].should eq(nil)

        cmd = "/_api/blueprints/edge?graph=#{graph_name}"
      	body = "{\"$id\" : \"edge1\", \"_from\" : \"vert2\", \"_to\" : \"vert1\", \"optional1\" : \"val1\"}"
        doc1 = ArangoDB.log_post("#{prefix}", cmd, :body => body)

      	doc1.code.should eq(400)
      	doc1.parsed_response['error'].should eq(true)
	      doc1.parsed_response['code'].should eq(400)
      end

      it "checks create edge with unknown vertex" do
        cmd = "/_api/blueprints/edge?graph=#{graph_name}"
      	body = "{\"$id\" : \"edge1\", \"_from\" : \"unknown\", \"_to\" : \"vert1\", \"optional1\" : \"val1\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

      	doc.code.should eq(400)
      	doc.parsed_response['error'].should eq(true)
	      doc.parsed_response['code'].should eq(400)
      end

      it "checks create edge with \$label" do
        cmd = "/_api/blueprints/edge?graph=#{graph_name}"
      	body = "{\"$id\" : \"edge1\", \"_from\" : \"vert2\", \"_to\" : \"vert1\", \"$label\" : \"label1\", \"optional1\" : \"val1\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

      	doc.code.should eq(200)
      	doc.parsed_response['error'].should eq(false)
	      doc.parsed_response['code'].should eq(200)
	      doc.parsed_response['edge']['$id'].should eq("edge1")
	      doc.parsed_response['edge']['optional1'].should eq("val1")
	      doc.parsed_response['edge']['$label'].should eq("label1")
      end

      it "checks create edge with _id of vertex" do
        cmd = "/_api/blueprints/vertex?graph=#{graph_name}"
      	body = "{}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
        v_id1 = doc.parsed_response['vertex']['_id']
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
        v_id2 = doc.parsed_response['vertex']['_id']

        cmd = "/_api/blueprints/edge?graph=#{graph_name}"
      	body = "{\"$id\" : \"edge1\", \"_from\" : \"#{v_id1}\", \"_to\" : \"#{v_id2}\", \"$label\" : \"label1\", \"optional1\" : \"val1\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

      	doc.code.should eq(200)
      	doc.parsed_response['error'].should eq(false)
	      doc.parsed_response['code'].should eq(200)
	      doc.parsed_response['edge']['$id'].should eq("edge1")
	      doc.parsed_response['edge']['optional1'].should eq("val1")
	      doc.parsed_response['edge']['_from'].should eq(v_id1)
	      doc.parsed_response['edge']['_to'].should eq(v_id2)
      end

      it "checks get edge by $id" do
        cmd = "/_api/blueprints/edge?graph=#{graph_name}"
      	body = "{\"$id\" : \"edge1\", \"_from\" : \"vert2\", \"_to\" : \"vert1\", \"optional1\" : \"val1\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
        eid = doc.parsed_response['edge']['$id']
        e_id = doc.parsed_response['edge']['_id']


        cmd = "/_api/blueprints/edge/#{eid}?graph=#{graph_name}"
        doc1 = ArangoDB.log_get("#{prefix}", cmd)

      	doc1.code.should eq(200)
      	doc1.parsed_response['error'].should eq(false)
	      doc1.parsed_response['code'].should eq(200)
	      doc1.parsed_response['edge']['$id'].should eq(eid)
	      doc1.parsed_response['edge']['_id'].should eq(e_id)
	      doc1.parsed_response['edge']['optional1'].should eq("val1")
	      doc1.parsed_response['edge']['$label'].should eq(nil)
      end

      it "checks get edge by _id" do
        cmd = "/_api/blueprints/edge?graph=#{graph_name}"
      	body = "{\"$id\" : \"edge1\", \"_from\" : \"vert2\", \"_to\" : \"vert1\", \"optional1\" : \"val1\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
        eid = doc.parsed_response['edge']['$id']
        e_id = doc.parsed_response['edge']['_id']


        cmd = "/_api/blueprints/edge/#{e_id}?graph=#{graph_name}"
        doc1 = ArangoDB.log_get("#{prefix}", cmd)

      	doc1.code.should eq(200)
      	doc1.parsed_response['error'].should eq(false)
	      doc1.parsed_response['code'].should eq(200)
	      doc1.parsed_response['edge']['$id'].should eq(eid)
	      doc1.parsed_response['edge']['_id'].should eq(e_id)
	      doc1.parsed_response['edge']['optional1'].should eq("val1")
	      doc1.parsed_response['edge']['$label'].should eq(nil)
      end

      it "checks replace edge properties by _id" do
        cmd = "/_api/blueprints/edge?graph=#{graph_name}"
      	body = "{\"$id\" : \"edge1\", \"_from\" : \"vert2\", \"_to\" : \"vert1\", \"optional1\" : \"val1\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
      	doc.code.should eq(200)
      	doc.parsed_response['error'].should eq(false)
	      doc.parsed_response['code'].should eq(200)
	      doc.parsed_response['edge']['optional1'].should eq("val1")
	      doc.parsed_response['edge']['optional2'].should eq(nil)
	      doc.parsed_response['edge']['$label'].should eq(nil)
        eid = doc.parsed_response['edge']['$id']
        e_id = doc.parsed_response['edge']['_id']
        e_to = doc.parsed_response['edge']['_to']


        cmd = "/_api/blueprints/edge/#{e_id}?graph=#{graph_name}"
      	body = "{\"$id\" : \"edge4711\", \"optional2\" : \"val2\", \"label\" : \"label1\", \"_to\" : \"to\"}"
        doc1 = ArangoDB.log_put("#{prefix}", cmd, :body => body)
      	doc1.code.should eq(200)
      	doc1.parsed_response['error'].should eq(false)
	      doc1.parsed_response['code'].should eq(200)
	      doc1.parsed_response['edge']['$id'].should eq(eid)
	      doc1.parsed_response['edge']['_id'].should eq(e_id)
	      doc1.parsed_response['edge']['_to'].should eq(e_to)
	      doc1.parsed_response['edge']['nameEdges'].should eq(nil)
	      doc1.parsed_response['edge']['optional1'].should eq(nil)
	      doc1.parsed_response['edge']['optional2'].should eq("val2")
	      doc1.parsed_response['edge']['$label'].should eq(nil)
      end

      it "checks delete edge by $id" do
        cmd = "/_api/blueprints/edge?graph=#{graph_name}"
      	body = "{\"$id\" : \"edge1\", \"_from\" : \"vert2\", \"_to\" : \"vert1\", \"optional1\" : \"val1\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
        eid = doc.parsed_response['edge']['$id']
        e_id = doc.parsed_response['edge']['_id']


        cmd = "/_api/blueprints/edge/#{eid}?graph=#{graph_name}"
        doc1 = ArangoDB.log_delete("#{prefix}", cmd)
      	doc1.code.should eq(200)
      	doc1.parsed_response['error'].should eq(false)
	      doc1.parsed_response['code'].should eq(200)

        cmd = "/_api/blueprints/edge/#{eid}?graph=#{graph_name}"
        doc2 = ArangoDB.log_get("#{prefix}", cmd)
      	doc2.code.should eq(400)
      	doc2.parsed_response['error'].should eq(true)
	      doc2.parsed_response['code'].should eq(400)
      end

      it "checks delete edge by _id" do
        cmd = "/_api/blueprints/edge?graph=#{graph_name}"
      	body = "{\"$id\" : \"edge1\", \"_from\" : \"vert2\", \"_to\" : \"vert1\", \"optional1\" : \"val1\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
        eid = doc.parsed_response['edge']['$id']
        e_id = doc.parsed_response['edge']['_id']

        cmd = "/_api/blueprints/edge/#{e_id}?graph=#{graph_name}"
        doc1 = ArangoDB.log_delete("#{prefix}", cmd)
      	doc1.code.should eq(200)
      	doc1.parsed_response['error'].should eq(false)
	      doc1.parsed_response['code'].should eq(200)

        cmd = "/_api/blueprints/edge/#{eid}?graph=#{graph_name}"
        doc2 = ArangoDB.log_get("#{prefix}", cmd)
      	doc2.code.should eq(400)
      	doc2.parsed_response['error'].should eq(true)
	      doc2.parsed_response['code'].should eq(400)
      end

    end

################################################################################
## checking edges responses
################################################################################

    context "checks blueprints edges requests" do
      before do
        truncate_collection(prefix, "_graphs")
        cmd = "/_api/blueprints/graph"
      	body = "{\"name\" : \"#{graph_name}\", \"verticesName\" : \"#{vertex_collection}\", \"edgesName\" : \"#{edge_collection}\"}"
        ArangoDB.post(cmd, :body => body)

        cmd = "/_api/blueprints/vertex?graph=#{graph_name}"
      	body = "{\"$id\" : \"id1\", \"optional1\" : \"val1a\", \"optional2\" : \"val2a\"}"
        ArangoDB.post(cmd, :body => body)
      	body = "{\"$id\" : \"id2\", \"optional1\" : \"val1b\", \"optional2\" : \"val2b\"}"
        ArangoDB.post(cmd, :body => body)
      	body = "{\"$id\" : \"id3\", \"optional1\" : \"val1c\", \"optional2\" : \"val2c\"}"
        ArangoDB.post(cmd, :body => body)

        cmd = "/_api/blueprints/edge?graph=#{graph_name}"
      	body1 = "{\"$id\" : \"edge1\", \"_from\" : \"id1\", \"_to\" : \"id2\", \"optional1\" : \"val1a\"}"
        ArangoDB.post(cmd, :body => body1)

      	body2 = "{\"$id\" : \"edge2\", \"_from\" : \"id2\", \"_to\" : \"id3\", \"optional1\" : \"val1b\"}"
        ArangoDB.post(cmd, :body => body2)
      end

      after do
        truncate_collection(prefix, "_graphs")
        ArangoDB.drop_collection( vertex_collection )
        ArangoDB.drop_collection( edge_collection )
      end
      
      it "checks list of all edges" do
        cmd = "/_api/blueprints/edges?graph=#{graph_name}"
      	body = "{\"batchSize\" : 100}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

      	doc.code.should eq(201)
      	doc.parsed_response['error'].should eq(false)
	      doc.parsed_response['code'].should eq(201)
	      doc.parsed_response['result'].count.should eq(2)
      end

      it "checks list of all edges of one vertex" do
        cmd = "/_api/blueprints/edges?graph=#{graph_name}"
      	body = "{\"batchSize\" : 100, \"vertex\" : \"id1\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

      	doc.code.should eq(201)
      	doc.parsed_response['error'].should eq(false)
	      doc.parsed_response['code'].should eq(201)
	      doc.parsed_response['result'].count.should eq(1)
	      doc.parsed_response['result'][0]['$id'].should eq("edge1")

        cmd = "/_api/blueprints/edges?graph=#{graph_name}"
      	body = "{\"batchSize\" : 100, \"vertex\" : \"id2\"}"
        doc2 = ArangoDB.log_post("#{prefix}", cmd, :body => body)

      	doc2.code.should eq(201)
      	doc2.parsed_response['error'].should eq(false)
	      doc2.parsed_response['code'].should eq(201)
	      doc2.parsed_response['result'].count.should eq(2)
      end

      it "checks list of all in edges of one vertex" do
        cmd = "/_api/blueprints/edges?graph=#{graph_name}"
      	body = "{\"batchSize\" : 100, \"vertex\" : \"id2\", \"direction\" : \"in\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

      	doc.code.should eq(201)
      	doc.parsed_response['error'].should eq(false)
	      doc.parsed_response['code'].should eq(201)
	      doc.parsed_response['result'].count.should eq(1)
	      doc.parsed_response['result'][0]['$id'].should eq("edge1")
      end

      it "checks list of all out edges of one vertex" do
        cmd = "/_api/blueprints/edges?graph=#{graph_name}"
      	body = "{\"batchSize\" : 100, \"vertex\" : \"id2\", \"direction\" : \"out\"}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

      	doc.code.should eq(201)
      	doc.parsed_response['error'].should eq(false)
	      doc.parsed_response['code'].should eq(201)
	      doc.parsed_response['result'].count.should eq(1)
	      doc.parsed_response['result'][0]['$id'].should eq("edge2")
      end

    end

  end

end
