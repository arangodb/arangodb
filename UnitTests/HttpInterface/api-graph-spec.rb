# coding: utf-8

require 'rspec'
require './arangodb.rb'


describe ArangoDB do
  prefix = "api-graph"

  vertex_collection = "v"
  edge_collection = "e"
  graph_name = "graph1"

  context "testing graph methods:" do

  def truncate_collection (prefix, name)
    cmd = "/_api/collection/#{name}/truncate"
    ArangoDB.put(cmd)
  end

  def create_graph (prefix, name, vertices, edges) 
    cmd = "/_api/graph"
    body = "{\"_key\" : \"#{name}\", \"vertices\" : \"#{vertices}\", \"edges\" : \"#{edges}\"}"
    doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
    return doc
  end

  def create_vertex (prefix, graphName, body) 
    cmd = "/_api/graph/#{graphName}/vertex"
    doc = ArangoDB.post(cmd, :body => body)
    return doc
  end

  def patch_vertex (prefix, graphName, name, body, keepNull) 
    if keepNull == '' then
      cmd = "/_api/graph/#{graphName}/vertex/#{name}"
    else 
      cmd = "/_api/graph/#{graphName}/vertex/#{name}?keepNull=#{keepNull}"
    end

    doc = ArangoDB.log_patch("#{prefix}", cmd, :body => body)
    return doc
  end

  def create_simple_vertex (prefix, graphName, name) 
    cmd = "/_api/graph/#{graphName}/vertex"
    body = "{\"_key\" : \"#{name}\"}"
    doc = ArangoDB.post(cmd, :body => body)
    return doc
  end

  def create_edge (prefix, graphName, body) 
    cmd = "/_api/graph/#{graphName}/edge"
    doc = ArangoDB.post(cmd, :body => body)
    return doc
  end

  def get_vertex (prefix, graphName, name) 
    cmd = "/_api/graph/#{graphName}/vertex/#{name}"
    doc = ArangoDB.get(cmd)
    return doc
  end

  def get_edge (prefix, graphName, name) 
    cmd = "/_api/graph/#{graphName}/edge/#{name}"
    doc = ArangoDB.get(cmd)
    return doc
  end

  def patch_edge (prefix, graphName, name, body, keepNull) 
    if keepNull == '' then
      cmd = "/_api/graph/#{graphName}/edge/#{name}"
    else 
      cmd = "/_api/graph/#{graphName}/edge/#{name}?keepNull=#{keepNull}"
    end

    doc = ArangoDB.log_patch("#{prefix}", cmd, :body => body)
    return doc
  end


################################################################################
## checking graph responses
################################################################################

    context "checks graph requests" do
      before do
        truncate_collection(prefix, "_graphs")

        cmd = "/_api/collection/_graphs/properties"
        body = "{\"waitForSync\" : true}"
        doc = ArangoDB.put(cmd, :body => body)        
      end

      after do
        truncate_collection(prefix, "_graphs")

        cmd = "/_api/collection/_graphs/properties"
        body = "{\"waitForSync\" : true}"
        doc = ArangoDB.put(cmd, :body => body)        

        ArangoDB.drop_collection( vertex_collection )
        ArangoDB.drop_collection( edge_collection )
        ArangoDB.drop_collection( "#{vertex_collection}2" )
        ArangoDB.drop_collection( "#{edge_collection}2" )
      end
      
      it "checks create graph" do
        doc = create_graph( prefix, graph_name, vertex_collection, edge_collection )

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['graph']['_key'].should eq("#{graph_name}")
        doc.parsed_response['graph']['_rev'].should eq(doc.headers['etag'])
      end

      it "checks create graph with wrong characters" do
        doc = create_graph( prefix, "hjü/$", vertex_collection, edge_collection )

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
      end

      it "checks create graph with wrong edges collection" do
        ArangoDB.create_collection( edge_collection , 0, 2)
        doc = create_graph( prefix, "wrong_edge_collection", vertex_collection, edge_collection )

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
      end

      it "checks create graph with same edges collection" do
        doc = create_graph( prefix, "with_same_edge_colection1", vertex_collection, edge_collection )
        doc.code.should eq(201)

        doc = create_graph( prefix, "with_same_edge_colection2", "vertex33", edge_collection )

        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
      end

      it "checks (re)create graph same name" do
        doc1 = create_graph( prefix, "recreate", vertex_collection, edge_collection )
        doc1.code.should eq(201)

        doc2 = create_graph( prefix, "recreate", vertex_collection, edge_collection )
        doc2.code.should eq(400)
      end

      it "checks (re)create graph different name" do
        doc1 = create_graph( prefix, "recreate_1", vertex_collection, edge_collection )
        doc1.code.should eq(201)

        doc2 = create_graph( prefix, "recreate_2", vertex_collection, edge_collection )
        doc2.code.should eq(400)
        doc2.parsed_response['error'].should eq(true)
      end

      it "checks create and get graph" do
        doc1 = create_graph( prefix, graph_name, vertex_collection, edge_collection )
        doc1.code.should eq(201)
        etag = doc1.headers['etag']

        doc1.parsed_response['graph']['_key'].should eq(graph_name)
        doc1.parsed_response['graph']['_rev'].should eq(etag)

        cmd = "/_api/graph/#{graph_name}"
        doc2 = ArangoDB.log_get("#{prefix}", cmd)

        doc2.code.should eq(200)
        doc2.parsed_response['graph']['_key'].should eq(graph_name)
        doc2.parsed_response['graph']['_rev'].should eq(etag)
        doc2.parsed_response['graph']['_rev'].should eq(doc2.headers['etag'])

        cmd = "/_api/graph/#{graph_name}?rev=#{etag}"
        doc3 = ArangoDB.log_get("#{prefix}", cmd)
        doc3.code.should eq(200)
        doc3.parsed_response['graph']['_key'].should eq(graph_name)
        doc3.parsed_response['graph']['_rev'].should eq(etag)
        doc3.parsed_response['graph']['_rev'].should eq(doc3.headers['etag'])

        cmd = "/_api/graph/#{graph_name}?rev=1007"
        doc4 = ArangoDB.log_get("#{prefix}", cmd)
        doc4.code.should eq(412)

        cmd = "/_api/graph/#{graph_name}?testWith=if-match"
        hdr = { "if-match" => "#{etag}" }
        doc5 = ArangoDB.log_get("#{prefix}", cmd, :headers => hdr)
        doc5.code.should eq(200)

        cmd = "/_api/graph/#{graph_name}?testWith=if-match2"
        hdr = { "if-match" => "2007" }
        doc5 = ArangoDB.log_get("#{prefix}", cmd, :headers => hdr)
        doc5.code.should eq(412)

        cmd = "/_api/graph/#{graph_name}?testWith=if-none-match"
        hdr = { "if-none-match" => "#{etag}" }
        doc5 = ArangoDB.log_get("#{prefix}", cmd, :headers => hdr)
        doc5.code.should eq(304)
      end

      it "checks create and get graph with waitForSync" do

        cmd = "/_api/collection/_graphs/properties"
        body = "{\"waitForSync\" : false}"
        doc = ArangoDB.put(cmd, :body => body)        

        doc1 = create_graph( prefix, graph_name, vertex_collection, edge_collection )
        doc1.code.should eq(202)
        etag = doc1.headers['etag']

        doc1.parsed_response['graph']['_key'].should eq(graph_name)
        doc1.parsed_response['graph']['_rev'].should eq(etag)

        cmd = "/_api/graph?waitForSync=true"
        body = "{\"_key\" : \"#{graph_name}2\", \"vertices\" : \"#{vertex_collection}2\", \"edges\" : \"#{edge_collection}2\"}"
        doc2 = ArangoDB.log_post("#{prefix}", cmd, :body => body)
        doc2.code.should eq(201)
      end

      it "checks create and delete graph" do
        # create
        doc1 = create_graph( prefix, graph_name, vertex_collection, edge_collection )
        doc1.code.should eq(201)

        # delete
        cmd = "/_api/graph/#{graph_name}"
        doc2 = ArangoDB.log_delete("#{prefix}", cmd)
        doc2.code.should eq(200)

        # check
        doc3 = ArangoDB.log_get("#{prefix}", cmd)
        doc3.code.should eq(404)
      end

      it "checks create and delete graph waitForSync" do

        cmd = "/_api/collection/_graphs/properties"
        body = "{\"waitForSync\" : false}"
        doc = ArangoDB.put(cmd, :body => body)        

        # create
        doc1 = create_graph( prefix, graph_name, vertex_collection, edge_collection )
        doc1.code.should eq(202)

        # delete
        cmd = "/_api/graph/#{graph_name}"
        doc2 = ArangoDB.log_delete("#{prefix}", cmd)
        doc2.code.should eq(202)

        # check
        doc3 = ArangoDB.log_get("#{prefix}", cmd)
        doc3.code.should eq(404)
      end

    end

################################################################################
## checking vertex responses
################################################################################

    context "checks graph vertex requests" do
      before do
        truncate_collection(prefix, "_graphs")

        cmd = "/_api/collection/_graphs/properties"
        body = "{\"waitForSync\" : true}"
        doc = ArangoDB.put(cmd, :body => body)        

        doc = create_graph( prefix, graph_name, vertex_collection, edge_collection )

        cmd = "/_api/collection/#{vertex_collection}/properties"
        body = "{\"waitForSync\" : true}"
        doc = ArangoDB.put(cmd, :body => body)    
      end

      after do
        truncate_collection(prefix, "_graphs")

        cmd = "/_api/collection/_graphs/properties"
        body = "{\"waitForSync\" : true}"
        doc = ArangoDB.put(cmd, :body => body)        

        ArangoDB.drop_collection( vertex_collection )
        ArangoDB.drop_collection( edge_collection )
      end
      
      it "checks create vertex with _key" do
        body = "{\"\_key\" : \"vertexTest1\", \"optional1\" : \"val1\", \"optional2\" : \"val2\"}"        
        doc = create_vertex( prefix, graph_name, body )

        doc.code.should eq(201)
        etag = doc.headers['etag']
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['vertex']['_key'].should eq("vertexTest1")
        doc.parsed_response['vertex']['optional1'].should eq("val1")
        doc.parsed_response['vertex']['_rev'].should eq(etag)
      end

      it "checks create vertex with _key and waitForSync" do
        cmd = "/_api/collection/_graphs/properties"
        body = "{\"waitForSync\" : false}"
        doc = ArangoDB.put(cmd, :body => body)        

        body = "{\"\_key\" : \"vertexTest1\", \"optional1\" : \"val1\", \"optional2\" : \"val2\"}"        
        doc = create_vertex( prefix, graph_name, body )

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['vertex']['_key'].should eq("vertexTest1")
        doc.parsed_response['vertex']['optional1'].should eq("val1")

        cmd = "/_api/graph/#{graph_name}/vertex?waitForSync=true"
        body = "{\"\_key\" : \"vertexTest2\", \"optional1\" : \"val1\", \"optional2\" : \"val2\"}"        
        doc2 = ArangoDB.log_post("#{prefix}", cmd, :body => body)

        doc2.code.should eq(201)
        doc2.parsed_response['error'].should eq(false)
        doc2.parsed_response['code'].should eq(201)
        doc2.parsed_response['vertex']['_key'].should eq("vertexTest2")
        doc2.parsed_response['vertex']['optional1'].should eq("val1")

      end

      it "checks create vertex with unknown graph" do
        body = "{\"\_key\" : \"vertexTest1\", \"optional1\" : \"val1\", \"optional2\" : \"val2\"}"        
        doc = create_vertex( prefix, "hugo", body )

        doc.code.should eq(404)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
      end

      it "checks create second vertex with same _key" do
        body = "{\"\_key\" : \"vertexTest2\", \"optional1\" : \"val1\", \"optional2\" : \"val2\"}"        
        doc = create_vertex( prefix, graph_name, body )

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)

        doc2 = create_vertex( prefix, graph_name, body )
        doc2.code.should eq(400)
        doc2.parsed_response['error'].should eq(true)
        doc2.parsed_response['code'].should eq(400)
      end

      it "checks get vertex by _key" do
        body = "{\"\_key\" : \"vertexTest3\", \"optional1\" : \"val1\", \"optional2\" : \"val2\"}"        
        doc = create_vertex( prefix, graph_name, body )

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['vertex']['_key'].should eq("vertexTest3")
        doc.parsed_response['vertex']['optional1'].should eq("val1")
        etag = doc.headers['etag'];

        doc2 = get_vertex( prefix, graph_name, "vertexTest3")
        doc2.code.should eq(200)
        doc2.parsed_response['error'].should eq(false)
        doc2.parsed_response['code'].should eq(200)
        doc2.parsed_response['vertex']['optional1'].should eq(doc.parsed_response['vertex']['optional1'])

        cmd = "/_api/graph/#{graph_name}/vertex/vertexTest3?rev=1007"
        doc4 = ArangoDB.log_get("#{prefix}", cmd)
        doc4.code.should eq(412)

        cmd = "/_api/graph/#{graph_name}/vertex/vertexTest3?testWith=if-match"
        hdr = { "if-match" => "#{etag}" }
        doc5 = ArangoDB.log_get("#{prefix}", cmd, :headers => hdr)
        doc5.code.should eq(200)

        cmd = "/_api/graph/#{graph_name}/vertex/vertexTest3?testWith=if-match2"
        hdr = { "if-match" => "2007" }
        doc6 = ArangoDB.log_get("#{prefix}", cmd, :headers => hdr)
        doc6.code.should eq(412)

        cmd = "/_api/graph/#{graph_name}/vertex/vertexTest3?testWith=if-none-match"
        hdr = { "if-none-match" => "#{etag}" }
        doc7 = ArangoDB.log_get("#{prefix}", cmd, :headers => hdr)
        doc7.code.should eq(304)
      end

      it "checks get vertex by _id" do
        body = "{\"\_key\" : \"vertexTest_id\", \"optional1\" : \"val1\", \"optional2\" : \"val2\"}"        
        doc = create_vertex( prefix, graph_name, body )

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        _id = doc.parsed_response['vertex']['_id'];

        doc2 = get_vertex( prefix, graph_name, _id)
        doc2.code.should eq(200)
        doc2.parsed_response['error'].should eq(false)
        doc2.parsed_response['code'].should eq(200)
        doc2.parsed_response['vertex']['optional1'].should eq(doc.parsed_response['vertex']['optional1'])
      end

      it "checks get vertex by wrong _key" do
        doc = get_vertex( prefix, graph_name, "vvv111")
        doc.code.should eq(404)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
      end

      it "checks update vertex by _key" do
        body = "{\"\_key\" : \"vertexTest5\", \"optional1\" : \"val1\"}"
        doc = create_vertex( prefix, graph_name, body )

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['vertex']['_key'].should eq("vertexTest5")
        doc.parsed_response['vertex']['optional1'].should eq("val1")

        doc1 = get_vertex( prefix, graph_name, "vertexTest5")
        doc1.code.should eq(200)
        doc1.parsed_response['vertex']['optional1'].should eq("val1")

        cmd = "/_api/graph/#{graph_name}/vertex/vertexTest5"
        body = "{\"optional1\" : \"val2\"}"
        doc3 = ArangoDB.log_put("#{prefix}", cmd, :body => body)
        doc3.code.should eq(201)
        doc3.parsed_response['error'].should eq(false)
        doc3.parsed_response['code'].should eq(201)
        doc3.parsed_response['vertex']['optional1'].should eq("val2")

        doc2 = get_vertex( prefix, graph_name, "vertexTest5")
        doc2.code.should eq(200)
        doc2.parsed_response['vertex']['optional1'].should eq("val2")

        cmd = "/_api/graph/#{graph_name}/vertex/error7777"
        body = "{\"optional1\" : \"val2\"}"
        docError = ArangoDB.log_put("#{prefix}", cmd, :body => body)
        docError.code.should eq(404)
        docError.parsed_response['error'].should eq(true)
        docError.parsed_response['code'].should eq(404)
      end

      it "checks update vertex" do
        body = "{\"optional1\" : \"val1\", \"optional2\" : \"val2\"}"
        doc = create_vertex( prefix, graph_name, body )

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['vertex']['optional1'].should eq("val1")
        doc.parsed_response['vertex']['optional2'].should eq("val2")
        _key = doc.parsed_response['vertex']['_key'];

        doc2 = get_vertex( prefix, graph_name, _key)
        doc2.code.should eq(200)
        doc2.parsed_response['error'].should eq(false)
        doc2.parsed_response['code'].should eq(200)
        doc2.parsed_response['vertex']['optional1'].should eq(doc.parsed_response['vertex']['optional1'])

        cmd = "/_api/graph/#{graph_name}/vertex/#{_key}"
        body = "{\"optional1\" : \"val2\"}"
        doc3 = ArangoDB.log_put("#{prefix}", cmd, :body => body)
        doc3.code.should eq(201)
        doc3.parsed_response['error'].should eq(false)
        doc3.parsed_response['code'].should eq(201)
        doc3.parsed_response['vertex']['optional1'].should eq("val2")
        doc3.parsed_response['vertex']['optional2'].should eq(nil)

        doc4 = get_vertex( prefix, graph_name, _key)
        doc4.code.should eq(200)
        doc4.parsed_response['error'].should eq(false)
        doc4.parsed_response['code'].should eq(200)
        doc4.parsed_response['vertex']['optional1'].should eq("val2")
        doc4.parsed_response['vertex']['optional2'].should eq(nil)
      end

      it "checks delete vertex" do
        body = "{\"optional1\" : \"vertexDelete1\"}"
        doc = create_vertex( prefix, graph_name, body )

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        _key = doc.parsed_response['vertex']['_key'];

        doc3 = get_vertex( prefix, graph_name, _key)
        doc3.code.should eq(200)
        doc3.parsed_response['error'].should eq(false)

        cmd = "/_api/graph/#{graph_name}/vertex/#{_key}"
        doc2 = ArangoDB.log_delete("#{prefix}", cmd)
        doc2.code.should eq(200)
        doc2.parsed_response['error'].should eq(false)
        doc2.parsed_response['code'].should eq(200)
        doc2.parsed_response['deleted'].should eq(true)

        doc4 = get_vertex( prefix, graph_name, _key)
        doc4.code.should eq(404)
        doc4.parsed_response['error'].should eq(true)
        doc4.parsed_response['code'].should eq(404)
      end

      it "checks patch vertex" do
        body = "{\"optional1\" : \"vertexPatch1\"}"
        doc = create_vertex( prefix, graph_name, body )

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['vertex']['optional1'].should eq('vertexPatch1')
        _key = doc.parsed_response['vertex']['_key'];

        body = "{\"optional2\" : \"vertexPatch2\"}"
        doc3 = patch_vertex(prefix, graph_name, _key, body, '')
        doc3.code.should eq(201)
        doc3.parsed_response['error'].should eq(false)
        doc3.parsed_response['vertex']['optional1'].should eq('vertexPatch1')
        doc3.parsed_response['vertex']['optional2'].should eq('vertexPatch2')

        cmd = "/_api/collection/#{vertex_collection}/properties"
        body = "{\"waitForSync\" : false}"
        doc = ArangoDB.put(cmd, :body => body)        

        body = "{\"_key\":\"egal\", \"optional2\" : null}"
        doc2 = patch_vertex(prefix, graph_name, _key, body, '')
        doc2.code.should eq(202)
        doc2.parsed_response['error'].should eq(false)
        doc2.parsed_response['code'].should eq(202)
        doc2.parsed_response['vertex']['optional1'].should eq('vertexPatch1')
        doc2.parsed_response['vertex']['optional2'].should be_nil
        doc2.parsed_response['vertex']['_key'].should eq(_key)

        body = ""
        doc4 = patch_vertex(prefix, graph_name, _key, body, '')
        doc4.code.should eq(202)
        doc4.parsed_response['error'].should eq(false)
        doc4.parsed_response['code'].should eq(202)
        doc4.parsed_response['vertex']['optional1'].should eq('vertexPatch1')
        doc4.parsed_response['vertex']['optional2'].should be_nil
        doc2.parsed_response['vertex']['_key'].should eq(_key)

        body = "{\"_key\":\"egal\", \"optional2\" : null}"
        doc5 = patch_vertex(prefix, graph_name, _key, body, 'false')
        doc5.code.should eq(202)
        doc5.parsed_response['error'].should eq(false)
        doc5.parsed_response['code'].should eq(202)
        doc5.parsed_response['vertex']['optional1'].should eq('vertexPatch1')
        doc2.parsed_response['vertex']['_key'].should eq(_key)
        doc5.parsed_response['vertex'].should_not have_key('optional2')

        body = "error in body"
        doc6 = patch_vertex(prefix, graph_name, _key, body, 'false')
        doc6.code.should eq(400)

        body = "{\"optional1\" : \"val2\"}"
        docError = patch_vertex(prefix, graph_name, "error999", body, 'false')
        docError.code.should eq(404)
        docError.parsed_response['error'].should eq(true)
        docError.parsed_response['code'].should eq(404)
      end

    end

################################################################################
## checking edge responses
################################################################################

    context "checks edge requests" do
      before do
        truncate_collection(prefix, "_graphs")

        cmd = "/_api/collection/_graphs/properties"
        body = "{\"waitForSync\" : true}"
        doc = ArangoDB.put(cmd, :body => body)        

        doc = create_graph( prefix, graph_name, vertex_collection, edge_collection )
        create_simple_vertex( prefix, graph_name, "vert1" )
        create_simple_vertex( prefix, graph_name, "vert2" )
        create_simple_vertex( prefix, graph_name, "vert3" )

        cmd = "/_api/collection/#{edge_collection}/properties"
        body = "{\"waitForSync\" : true}"
        doc = ArangoDB.put(cmd, :body => body)    
      end

      after do

        cmd = "/_api/collection/_graphs/properties"
        body = "{\"waitForSync\" : true}"
        doc = ArangoDB.put(cmd, :body => body)        

        truncate_collection(prefix, "_graphs")
        ArangoDB.drop_collection( vertex_collection )
        ArangoDB.drop_collection( edge_collection )
      end
      
      it "checks create edge" do
        body = "{\"_key\" : \"edgeTest1\", \"_from\" : \"vert2\", \"_to\" : \"vert1\", \"optional1\" : \"val1\"}"
        doc = create_edge( prefix, graph_name, body )

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['edge']['_key'].should eq("edgeTest1")
        doc.parsed_response['edge']['optional1'].should eq("val1")
        doc.parsed_response['edge']['$label'].should eq(nil)
      end

      it "checks create second edge with same \_key" do
        body = "{\"_key\" : \"edgeTest2\", \"_from\" : \"vert2\", \"_to\" : \"vert1\", \"optional1\" : \"val1\"}"
        doc = create_edge( prefix, graph_name, body )

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['edge']['_key'].should eq("edgeTest2")
        doc.parsed_response['edge']['optional1'].should eq("val1")
        doc.parsed_response['edge']['$label'].should eq(nil)

        doc1 = create_edge( prefix, graph_name, body )
        doc1.code.should eq(400)
        doc1.parsed_response['error'].should eq(true)
        doc1.parsed_response['code'].should eq(400)
      end

      it "checks create edge with unknown vertex" do
        body = "{\"_key\" : \"edgeTest3\", \"_from\" : \"unknownVertex\", \"_to\" : \"vert1\", \"optional1\" : \"val1\"}"
        doc = create_edge( prefix, graph_name, body )
        doc.code.should eq(400)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
      end

      it "checks create edge with \$label" do
        body = "{\"_key\" : \"edgeTest4\", \"_from\" : \"vert2\", \"_to\" : \"vert1\", \"$label\" : \"label1\", \"optional1\" : \"val1\"}"
        doc = create_edge( prefix, graph_name, body )

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['edge']['_key'].should eq("edgeTest4")
        doc.parsed_response['edge']['optional1'].should eq("val1")
        doc.parsed_response['edge']['$label'].should eq("label1")
      end

      it "checks create edge with _id of vertex" do
        doc = create_simple_vertex( prefix, graph_name, "vert1a" )
        v_id1 = doc.parsed_response['vertex']['_id']

        doc = create_simple_vertex( prefix, graph_name, "vert2a" )
        v_id2 = doc.parsed_response['vertex']['_id']

        body = "{\"_key\" : \"edgeTest5\", \"_from\" : \"#{v_id1}\", \"_to\" : \"#{v_id2}\", \"$label\" : \"label1\", \"optional1\" : \"val1\"}"
        doc = create_edge( prefix, graph_name, body )

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['edge']['_key'].should eq("edgeTest5")
        doc.parsed_response['edge']['optional1'].should eq("val1")
        doc.parsed_response['edge']['_from'].should eq(v_id1)
        doc.parsed_response['edge']['_to'].should eq(v_id2)
      end

      it "checks get edge by _id" do
        body = "{\"_key\" : \"edgeTest6\", \"_from\" : \"vert2\", \"_to\" : \"vert1\", \"$label\" : \"label1\", \"optional1\" : \"val1\"}"
        doc = create_edge( prefix, graph_name, body )

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['edge']['_key'].should eq("edgeTest6")
        doc.parsed_response['edge']['optional1'].should eq("val1")
        doc.parsed_response['edge']['$label'].should eq("label1")

        e_key = doc.parsed_response['edge']['_key']
        e_id  = doc.parsed_response['edge']['_id']

        doc1 = get_edge( prefix, graph_name, e_key )

        doc1.code.should eq(200)
        doc1.parsed_response['error'].should eq(false)
        doc1.parsed_response['code'].should eq(200)
        doc1.parsed_response['edge']['_id'].should eq(e_id)

        doc2 = get_edge( prefix, graph_name, e_id )

        doc2.code.should eq(200)
        doc2.parsed_response['error'].should eq(false)
        doc2.parsed_response['code'].should eq(200)
        doc2.parsed_response['edge']['_id'].should eq(e_id)
      end

      it "checks replace edge properties by _id" do
        body = "{\"_key\" : \"edgeTest7\", \"_from\" : \"vert2\", \"_to\" : \"vert1\", \"$label\" : \"label1\", \"optional1\" : \"val1\"}"
        doc = create_edge( prefix, graph_name, body )
        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['edge']['optional1'].should eq("val1")
        doc.parsed_response['edge']['optional2'].should eq(nil)

        e_key = doc.parsed_response['edge']['_key']
        e_id = doc.parsed_response['edge']['_id']
        e_to = doc.parsed_response['edge']['_to']

        cmd = "/_api/graph/#{graph_name}/edge/#{e_id}"
        body = "{\"_key\" : \"edge4711\", \"optional2\" : \"val2\", \"$label\" : \"label2\", \"_to\" : \"to\"}"
        doc1 = ArangoDB.log_put("#{prefix}", cmd, :body => body)
        doc1.code.should eq(201)
        doc1.parsed_response['error'].should eq(false)
        doc1.parsed_response['code'].should eq(201)
        doc1.parsed_response['edge']['_key'].should eq(e_key)
        doc1.parsed_response['edge']['_id'].should eq(e_id)
        doc1.parsed_response['edge']['_to'].should eq(e_to)
        doc1.parsed_response['edge']['optional1'].should eq(nil)
        doc1.parsed_response['edge']['optional2'].should eq("val2")
        doc1.parsed_response['edge']['$label'].should eq("label1")

        cmd = "/_api/graph/#{graph_name}/edge/error4711"
        body = "{\"_key\" : \"edge4711\", \"optional2\" : \"val2\", \"$label\" : \"label2\", \"_to\" : \"to\"}"
        doc1 = ArangoDB.log_put("#{prefix}", cmd, :body => body)
        doc1.code.should eq(404)
        doc1.parsed_response['error'].should eq(true)
        doc1.parsed_response['code'].should eq(404)

      end

      it "checks delete edge by _id" do
        body = "{\"_key\" : \"edgeTest8\", \"_from\" : \"vert2\", \"_to\" : \"vert1\", \"$label\" : \"label1\", \"optional1\" : \"val1\"}"
        doc = create_edge( prefix, graph_name, body )
        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['edge']['optional1'].should eq("val1")
        doc.parsed_response['edge']['optional2'].should eq(nil)

        e_id = doc.parsed_response['edge']['_id']

        cmd = "/_api/graph/#{graph_name}/edge/#{e_id}"
        doc1 = ArangoDB.log_delete("#{prefix}", cmd)
        doc1.code.should eq(200)
        doc1.parsed_response['error'].should eq(false)
        doc1.parsed_response['code'].should eq(200)

        doc2 = get_edge( prefix, graph_name, e_id )
        doc2.code.should eq(404)
        doc2.parsed_response['error'].should eq(true)
        doc2.parsed_response['code'].should eq(404)
      end

      it "checks delete edge by _key" do
        body = "{\"_key\" : \"edgeTest9\", \"_from\" : \"vert2\", \"_to\" : \"vert1\", \"$label\" : \"label1\", \"optional1\" : \"val1\"}"
        doc = create_edge( prefix, graph_name, body )
        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['edge']['optional1'].should eq("val1")
        doc.parsed_response['edge']['optional2'].should eq(nil)

        cmd = "/_api/graph/#{graph_name}/edge/edgeTest9"
        doc1 = ArangoDB.log_delete("#{prefix}", cmd)
        doc1.code.should eq(200)
        doc1.parsed_response['error'].should eq(false)
        doc1.parsed_response['code'].should eq(200)

        doc2 = get_edge( prefix, graph_name, "edgeTest9" )
        doc2.code.should eq(404)
        doc2.parsed_response['error'].should eq(true)
        doc2.parsed_response['code'].should eq(404)

      end

      it "checks patch edge" do
        body = "{\"_key\" : \"patchEdge\", \"_from\" : \"vert2\", \"_to\" : \"vert1\", \"$label\" : \"label1\", \"optional1\" : \"val1\"}"
        doc = create_edge( prefix, graph_name, body )
        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['edge']['optional1'].should eq("val1")
        doc.parsed_response['edge'].should_not have_key('optional2')
        doc.parsed_response['edge']['$label'].should eq("label1")
        doc.parsed_response['edge']['_key'].should eq("patchEdge")
        _key = doc.parsed_response['edge']['_key'];

        body = "{\"$label\" : \"egal\", \"optional2\" : \"val2\"}"
        doc1 = patch_edge("#{prefix}", graph_name, _key, body, '');
        doc1.code.should eq(201)
        doc1.parsed_response['edge']['optional1'].should eq("val1")
        doc1.parsed_response['edge']['optional2'].should eq("val2")
        doc1.parsed_response['edge']['$label'].should eq("label1")
        doc1.parsed_response['edge']['_key'].should eq("patchEdge")
        
        cmd = "/_api/collection/#{edge_collection}/properties"
        body = "{\"waitForSync\" : false}"
        doc = ArangoDB.put(cmd, :body => body)    

        body = "{\"optional2\" : null}"
        doc2 = patch_edge("#{prefix}", graph_name, _key, body, '');
        doc2.code.should eq(202)
        doc2.parsed_response['edge']['optional1'].should eq("val1")
        doc2.parsed_response['edge']['optional2'].should be_nil
        doc2.parsed_response['edge']['$label'].should eq("label1")
        doc2.parsed_response['edge']['_key'].should eq("patchEdge")

        body = "{\"optional1\" : null}"
        doc3 = patch_edge("#{prefix}", graph_name, _key, body, 'false');
        doc3.code.should eq(202)
        doc3.parsed_response['edge'].should_not have_key('optional1')
        doc3.parsed_response['edge']['optional2'].should be_nil
        doc3.parsed_response['edge']['$label'].should eq("label1")
        doc3.parsed_response['edge']['_key'].should eq("patchEdge")

        body = ""
        doc4 = patch_edge("#{prefix}", graph_name, _key, body, '');
        doc4.code.should eq(202)
        doc3.parsed_response['edge'].should_not have_key('optional1')
        doc3.parsed_response['edge']['optional2'].should be_nil
        doc3.parsed_response['edge']['$label'].should eq("label1")
        doc3.parsed_response['edge']['_key'].should eq("patchEdge")

        body = "{\"optional1\" : \"val2\"}"
        docError = patch_edge(prefix, graph_name, "error999", body, 'false')
        docError.code.should eq(404)
        docError.parsed_response['error'].should eq(true)
        docError.parsed_response['code'].should eq(404)
      end

    end

################################################################################
## checking vertices responses
################################################################################

    context "checks vertices requests" do
      before do
        truncate_collection(prefix, "_graphs")

        cmd = "/_api/collection/_graphs/properties"
        body = "{\"waitForSync\" : true}"
        doc = ArangoDB.put(cmd, :body => body)        

        doc = create_graph( prefix, graph_name, vertex_collection, edge_collection )
        cmd = "/_api/graph/#{graph_name}/vertex"
        body = "{\"_key\" : \"id1\", \"optional1\" : \"val1\", \"optional2\" : 1}"
        ArangoDB.post(cmd, :body => body)
        body = "{\"_key\" : \"id2\", \"optional1\" : \"val1\", \"optional2\" : 2}"
        ArangoDB.post(cmd, :body => body)
        body = "{\"_key\" : \"id3\", \"optional1\" : \"val1\", \"optional2\" : 2}"
        ArangoDB.post(cmd, :body => body)
        body = "{\"_key\" : \"id4\", \"optional1\" : \"val1\", \"optional2\" : 3}"
        ArangoDB.post(cmd, :body => body)
        body = "{\"_key\" : \"id5\", \"optional2\" : \"val2\"}"
        ArangoDB.post(cmd, :body => body)

        cmd = "/_api/graph/#{graph_name}/edge"
        body = "{\"_key\" : \"edge1\", \"_from\" : \"id1\", \"_to\" : \"id2\", \"$label\" : \"l1\"}"
        doc = ArangoDB.post(cmd, :body => body)
        body = "{\"_key\" : \"edge2\", \"_from\" : \"id2\", \"_to\" : \"id3\", \"$label\" : \"l2\"}"
        doc = ArangoDB.post(cmd, :body => body)
      end

      after do
        truncate_collection(prefix, "_graphs")

        cmd = "/_api/collection/_graphs/properties"
        body = "{\"waitForSync\" : true}"
        doc = ArangoDB.put(cmd, :body => body)        

        ArangoDB.drop_collection( vertex_collection )
        ArangoDB.drop_collection( edge_collection )
      end
      
      it "checks list of vertices of a graph" do
        cmd = "/_api/graph/#{graph_name}/vertices"
        body = "{\"batchSize\" : 100 }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['result'].count.should eq(5)

        body = "{\"batchSize\" : 100, \"limit\" : 1 }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['result'].count.should eq(1)

        body = "{\"batchSize\" : 100, \"filter\" : { \"properties\" : [ { \"key\" : \"optional2\", \"value\" : 3 } ] } }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['result'].count.should eq(1)

        body = "{\"batchSize\" : 100,  \"filter\" : { \"properties\" : [ { \"key\" : \"optional2\", \"value\" : 2 } ] } }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['result'].count.should eq(2)
      end

      it "checks list of vertices of a vertex" do
        cmd = "/_api/graph/#{graph_name}/vertices"
        body = "{\"batchSize\" : 100 }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['result'].count.should eq(5)

        body = "{\"batchSize\" : 100, \"limit\" : 2 }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['result'].count.should eq(2)

        body = "{\"batchSize\" : 100, \"filter\" : { \"properties\" : [ { \"key\" : \"optional2\", \"value\" : 3 } ] } }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['result'].count.should eq(1)

        body = "{\"batchSize\" : 100,  \"filter\" : { \"properties\" : [ { \"key\" : \"optional2\", \"value\" : 2 } ] } }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['result'].count.should eq(2)
        
        cmd = "/_api/graph/#{graph_name}/vertices/id2"
        body = "{\"batchSize\" : 100 }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['result'].count.should eq(2)
        
        body = "{\"batchSize\" : 100, \"filter\" : { \"direction\" : \"in\" } }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['result'].count.should eq(1)
        doc.parsed_response['result'][0]['_key'].should eq("id1")

        body = "{\"batchSize\" : 100, \"filter\" : { \"direction\" : \"out\" } }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['result'].count.should eq(1)
        doc.parsed_response['result'][0]['_key'].should eq("id3")

        body = "{\"batchSize\" : 100, \"filter\" : { \"labels\" : [\"l2\"] } }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['result'].count.should eq(1)
        doc.parsed_response['result'][0]['_key'].should eq("id3")

      end

      it "checks list of vertices of a vertex with compare" do
        cmd = "/_api/graph/#{graph_name}/vertices"
        body = "{\"batchSize\" : 100, \"filter\" : { \"properties\" : [ { \"key\" : \"optional2\", \"value\" : 100 , \"compare\" : \"<\" } ] } }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)
        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['result'].count.should eq(4)
      end

    end

################################################################################
## checking edges responses
################################################################################

    context "checks edges requests" do
      before do
        truncate_collection(prefix, "_graphs")

        cmd = "/_api/collection/_graphs/properties"
        body = "{\"waitForSync\" : true}"
        doc = ArangoDB.put(cmd, :body => body)        

        doc = create_graph( prefix, graph_name, vertex_collection, edge_collection )
        cmd = "/_api/graph/#{graph_name}/vertex"
        body = "{\"_key\" : \"id1\", \"optional1\" : \"val1a\", \"optional2\" : \"val2a\"}"
        ArangoDB.post(cmd, :body => body)
        body = "{\"_key\" : \"id2\", \"optional1\" : \"val1b\", \"optional2\" : \"val2b\"}"
        ArangoDB.post(cmd, :body => body)
        body = "{\"_key\" : \"id3\", \"optional1\" : \"val1c\", \"optional2\" : \"val2c\"}"
        ArangoDB.post(cmd, :body => body)

        cmd = "/_api/graph/#{graph_name}/edge"
        body1 = "{\"_key\" : \"edge1\", \"_from\" : \"id1\", \"_to\" : \"id2\", \"optional1\" : \"val1a\"}"
        ArangoDB.post(cmd, :body => body1)
        body2 = "{\"_key\" : \"edge2\", \"_from\" : \"id2\", \"_to\" : \"id3\", \"optional1\" : \"val1b\"}"
        ArangoDB.post(cmd, :body => body2)
      end

      after do
        truncate_collection(prefix, "_graphs")

        cmd = "/_api/collection/_graphs/properties"
        body = "{\"waitForSync\" : true}"
        doc = ArangoDB.put(cmd, :body => body)        

        ArangoDB.drop_collection( vertex_collection )
        ArangoDB.drop_collection( edge_collection )
      end
      
      it "checks list of all edges" do
        cmd = "/_api/graph/#{graph_name}/edges"
        body = "{\"batchSize\" : 100}"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['result'].count.should eq(2)

        body = "{\"batchSize\" : 100, \"limit\" : 1 }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['result'].count.should eq(1)

      end

      it "checks list of all edges" do
        cmd = "/_api/graph/#{graph_name}/edges"
        body = "{\"batchSize\" : 100,  \"filter\" : { \"properties\" : [ { \"key\" : \"optional1\", \"value\" : \"val1a\" } ] } }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['result'].count.should eq(1)
      end

      it "checks list of all edges of one vertex" do
        cmd = "/_api/graph/#{graph_name}/edges/id1"
        body = "{\"batchSize\" : 100 }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['result'].count.should eq(1)
        doc.parsed_response['result'][0]['_key'].should eq("edge1")

        cmd = "/_api/graph/#{graph_name}/edges/id2"
        body = "{\"batchSize\" : 100 }"
        doc2 = ArangoDB.log_post("#{prefix}", cmd, :body => body)

        doc2.code.should eq(201)
        doc2.parsed_response['error'].should eq(false)
        doc2.parsed_response['code'].should eq(201)
        doc2.parsed_response['result'].count.should eq(2)
      end

      it "checks list of all in edges of one vertex" do
        cmd = "/_api/graph/#{graph_name}/edges/id2"
        body = "{\"batchSize\" : 100, \"filter\" : { \"direction\" : \"in\" } }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['result'].count.should eq(1)
        doc.parsed_response['result'][0]['_key'].should eq("edge1")
      end

      it "checks list of all out edges of one vertex" do
        cmd = "/_api/graph/#{graph_name}/edges/id2"
        body = "{\"batchSize\" : 100, \"filter\" : { \"direction\" : \"out\" } }"
        doc = ArangoDB.log_post("#{prefix}", cmd, :body => body)

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['result'].count.should eq(1)
        doc.parsed_response['result'][0]['_key'].should eq("edge2")
      end

    end

  end

end
