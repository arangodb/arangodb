# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_api/traversal"
  prefix = "api-traversal"

  context "running server-side traversals:" do

################################################################################
## setup
################################################################################

    before do
      @cv = "UnitTestsTraversalVertex"

      @ce = "UnitTestsTraversalEdge"

      @gn = "UnitTestsTraversalGraph"

      ArangoDB.create_single_collection_graph(@gn, @ce, @cv)

      cmd = "/_api/document?collection=#{@cv}" 
      [ 
        "World", "Nothing", "Europe", "Asia", "America", "Australia", "Antarctica", "Africa", "Blackhole", "Blackhole2", 
        "DE", "FR", "GB", "IE", "CN", "JP", "TW", "US", "MX", "AU", "EG", "ZA", "AN", 
        "London", "Paris", "Lyon", "Cologne","Dusseldorf", "Beijing", "Shanghai", "Tokyo", "Kyoto", "Taipeh", "Perth", "Sydney"
      ].each do|loc|
        body = "{ \"_key\" : \"#{loc}\" }"
        doc = ArangoDB.post(cmd, :body => body)
        doc.code.should eq(202)
      end
      
      count = 1000
      cmd = "/_api/document?collection=#{@ce}" 
      [
        ["World", "Europe"],
        ["World", "Asia"],
        ["World", "America"],
        ["World", "Australia"],
        ["Europe", "DE"],
        ["Europe", "FR"],
        ["Europe", "GB"],
        ["Asia", "CN"],
        ["Asia", "JP"],
        ["Asia", "TW"],
        ["America", "US"],
        ["America", "MX"],
        ["Australia", "AU"],
        ["Blackhole", "Blackhole2"],
        ["Blackhole2", "Blackhole"]
      ].each do|pair|
        count = count + 1
        from = pair[0]
        to = pair[1]
        body = "{ \"_key\" : \"#{count}\", \"_from\" : \"#{@cv}/#{from}\", \"_to\" : \"#{@cv}/#{to}\" }"
        doc = ArangoDB.post(cmd, :body => body)
        doc.code.should eq(202)
      end
    end
    
    after do
      ArangoDB.drop_graph(@gn)
    end

################################################################################
## errors
################################################################################

   context "error checking" do

################################################################################
## no direction, no expander
################################################################################
      
      it "no direction, no expander" do
        body = "{ \"graphName\" : \"#{@gn}\", \"startVertex\" : \"#{@cv}/World\" }"
        doc = ArangoDB.log_post("#{prefix}-visit-no-expander", api, :body => body)

        doc.code.should eq(400)

        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(400)
      end

################################################################################
## no edge collection
################################################################################
      
      it "no graph" do
        body = "{ \"startVertex\" : \"#{@cv}/World\", \"direction\" : \"outbound\" }"
        doc = ArangoDB.log_post("#{prefix}-visit-no-graph", api, :body => body)

        doc.code.should eq(400)

        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(400)
      end

################################################################################
## non existing edge collection
################################################################################
      
      it "non-existing graph" do
        body = "{ \"graphName\" : \"UnitTestsNonExistingGraph\", \"startVertex\" : \"#{@cv}/World\", \"direction\" : \"outbound\" }"
        doc = ArangoDB.log_post("#{prefix}-visit-non-existing-graph", api, :body => body)

        doc.code.should eq(404)

        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
        doc.parsed_response['errorNum'].should eq(1924)
      end

################################################################################
## no start vertex
################################################################################
      
      it "no start vertex" do
        body = "{ \"graphName\" : \"#{@gn}\" }"
        doc = ArangoDB.log_post("#{prefix}-visit-no-start-vertex", api, :body => body)

        doc.code.should eq(400)

        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(400)
      end

################################################################################
## invalid start vertex
################################################################################
      
      it "non-existing start vertex" do
        body = "{ \"graphName\" : \"#{@gn}\", \"startVertex\" : \"#{@cv}/nonexisting\", \"direction\" : \"outbound\" }"
        doc = ArangoDB.log_post("#{prefix}-visit-non-existing-vertex", api, :body => body)

        doc.code.should eq(404)

        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
        doc.parsed_response['errorNum'].should eq(1202)
      end

################################################################################
## invalid direction
################################################################################
      
      it "invalid direction" do
        body = "{ \"graphName\" : \"#{@gn}\", \"startVertex\" : \"#{@cv}/World\", \"direction\" : \"foo\" }"
        doc = ArangoDB.log_post("#{prefix}-visit-invalid-direction", api, :body => body)
        
        doc.code.should eq(400)

        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(400)
      end

################################################################################
## server exception
################################################################################
      
      it "traversal exception" do
        body = "{ \"graphName\" : \"#{@gn}\", \"startVertex\" : \"#{@cv}/World\", \"direction\" : \"outbound\", \"visitor\" : \"throw 'bang!';\" }"
        doc = ArangoDB.log_post("#{prefix}-visit-traversal-exception", api, :body => body)

        doc.code.should eq(500)

        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(500)
        doc.parsed_response['errorNum'].should eq(500)
      end

################################################################################
## traversal abortion
################################################################################
      
      it "traversal abortion, few iterations" do
        body = "{ \"graphName\" : \"#{@gn}\", \"startVertex\" : \"#{@cv}/Blackhole\", \"direction\" : \"outbound\", \"uniqueness\" : { \"vertices\" : \"none\", \"edges\" : \"none\" }, \"maxIterations\" : 5 }"
        doc = ArangoDB.log_post("#{prefix}-visit-traversal-abort1", api, :body => body)

        doc.code.should eq(500)

        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(500)
        doc.parsed_response['errorNum'].should eq(1909)
      end
      it "traversal abortion, many iterations" do
        body = "{ \"graphName\" : \"#{@gn}\", \"startVertex\" : \"#{@cv}/Blackhole\", \"direction\" : \"outbound\", \"uniqueness\" : { \"vertices\" : \"none\", \"edges\" : \"none\" }, \"maxIterations\" : 5000, \"maxDepth\" : 999999, \"visitor\" : \"\" }"
        doc = ArangoDB.log_post("#{prefix}-visit-traversal-abort2", api, :body => body)

        doc.code.should eq(500)

        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(500)
        doc.parsed_response['errorNum'].should eq(1909)
      end

      
    end

################################################################################
## outbound traversals
################################################################################

    context "outbound traversals" do

################################################################################
## depth-first, preorder, forward
################################################################################

      it "visits nodes in a graph, outbound, pre-order, forward" do
        body = "{ \"graphName\" : \"#{@gn}\", \"strategy\" : \"depthfirst\", \"order\" : \"preorder\", \"itemOrder\" : \"forward\", \"direction\" : \"outbound\", \"startVertex\" : \"#{@cv}/World\", \"visitor\" : \"result.visited.vertices.push(vertex._id); result.visited.paths.push(function() { var paths = [ ]; for (var i = 0; i < path.vertices.length; ++i) { paths.push(path.vertices[i]._id); } return paths;}());\", \"sort\" : \"if (l._key < r._key) { return -1; } else if (l._key > r._key) { return 1; } return 0;\" }"
        doc = ArangoDB.log_post("#{prefix}-preorder-forward", api, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
       
        doc.parsed_response['result']['visited']['vertices'].should eq([
          "#{@cv}/World",
          "#{@cv}/Europe",
          "#{@cv}/DE",
          "#{@cv}/FR",
          "#{@cv}/GB",
          "#{@cv}/Asia",
          "#{@cv}/CN",
          "#{@cv}/JP",
          "#{@cv}/TW",
          "#{@cv}/America",
          "#{@cv}/US",
          "#{@cv}/MX",
          "#{@cv}/Australia",
          "#{@cv}/AU"
        ])
        
        doc.parsed_response['result']['visited']['paths'].should eq([
          [ "#{@cv}/World" ],
          [ "#{@cv}/World", "#{@cv}/Europe" ],
          [ "#{@cv}/World", "#{@cv}/Europe", "#{@cv}/DE" ],
          [ "#{@cv}/World", "#{@cv}/Europe", "#{@cv}/FR" ],
          [ "#{@cv}/World", "#{@cv}/Europe", "#{@cv}/GB" ],
          [ "#{@cv}/World", "#{@cv}/Asia" ],
          [ "#{@cv}/World", "#{@cv}/Asia", "#{@cv}/CN" ],
          [ "#{@cv}/World", "#{@cv}/Asia", "#{@cv}/JP" ],
          [ "#{@cv}/World", "#{@cv}/Asia", "#{@cv}/TW" ],
          [ "#{@cv}/World", "#{@cv}/America" ],
          [ "#{@cv}/World", "#{@cv}/America", "#{@cv}/US" ],
          [ "#{@cv}/World", "#{@cv}/America", "#{@cv}/MX" ],
          [ "#{@cv}/World", "#{@cv}/Australia" ],
          [ "#{@cv}/World", "#{@cv}/Australia", "#{@cv}/AU" ]
        ])
      end

################################################################################
## depth-first, preorder, backward
################################################################################

      it "visits nodes in a graph, outbound, pre-order, backward" do
        body = "{ \"graphName\" : \"#{@gn}\", \"strategy\" : \"depthfirst\", \"order\" : \"preorder\", \"itemOrder\" : \"backward\", \"direction\" : \"outbound\", \"startVertex\" : \"#{@cv}/World\", \"visitor\" : \"result.visited.vertices.push(vertex._id); result.visited.paths.push(function() { var paths = [ ]; for (var i = 0; i < path.vertices.length; ++i) { paths.push(path.vertices[i]._id); } return paths;}());\", \"sort\" : \"if (l._key < r._key) { return -1; } else if (l._key > r._key) { return 1; } return 0;\" }"
        doc = ArangoDB.log_post("#{prefix}-visit-preorder-backward", api, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
       
        doc.parsed_response['result']['visited']['vertices'].should eq([
          "#{@cv}/World",
          "#{@cv}/Australia",
          "#{@cv}/AU",
          "#{@cv}/America",
          "#{@cv}/MX",
          "#{@cv}/US",
          "#{@cv}/Asia",
          "#{@cv}/TW",
          "#{@cv}/JP",
          "#{@cv}/CN",
          "#{@cv}/Europe",
          "#{@cv}/GB",
          "#{@cv}/FR",
          "#{@cv}/DE",
        ])
        
        doc.parsed_response['result']['visited']['paths'].should eq([
          [ "#{@cv}/World" ],
          [ "#{@cv}/World", "#{@cv}/Australia" ],
          [ "#{@cv}/World", "#{@cv}/Australia", "#{@cv}/AU" ],
          [ "#{@cv}/World", "#{@cv}/America" ],
          [ "#{@cv}/World", "#{@cv}/America", "#{@cv}/MX" ],
          [ "#{@cv}/World", "#{@cv}/America", "#{@cv}/US" ],
          [ "#{@cv}/World", "#{@cv}/Asia" ],
          [ "#{@cv}/World", "#{@cv}/Asia", "#{@cv}/TW" ],
          [ "#{@cv}/World", "#{@cv}/Asia", "#{@cv}/JP" ],
          [ "#{@cv}/World", "#{@cv}/Asia", "#{@cv}/CN" ],
          [ "#{@cv}/World", "#{@cv}/Europe" ],
          [ "#{@cv}/World", "#{@cv}/Europe", "#{@cv}/GB" ],
          [ "#{@cv}/World", "#{@cv}/Europe", "#{@cv}/FR" ],
          [ "#{@cv}/World", "#{@cv}/Europe", "#{@cv}/DE" ]
        ])
      end

################################################################################
## depth-first, postorder, forward
################################################################################

      it "visits nodes in a graph, outbound, post-order, forward" do
        body = "{ \"graphName\" : \"#{@gn}\", \"strategy\" : \"depthfirst\", \"order\" : \"postorder\", \"itemOrder\" : \"forward\", \"direction\" : \"outbound\", \"startVertex\" : \"#{@cv}/World\", \"visitor\" : \"result.visited.vertices.push(vertex._id); result.visited.paths.push(function() { var paths = [ ]; for (var i = 0; i < path.vertices.length; ++i) { paths.push(path.vertices[i]._id); } return paths;}());\", \"sort\" : \"if (l._key < r._key) { return -1; } else if (l._key > r._key) { return 1; } return 0;\" }"
        doc = ArangoDB.log_post("#{prefix}-visit-postorder-forward", api, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
       
        doc.parsed_response['result']['visited']['vertices'].should eq([
          "#{@cv}/DE",
          "#{@cv}/FR",
          "#{@cv}/GB",
          "#{@cv}/Europe",
          "#{@cv}/CN",
          "#{@cv}/JP",
          "#{@cv}/TW",
          "#{@cv}/Asia",
          "#{@cv}/US",
          "#{@cv}/MX",
          "#{@cv}/America",
          "#{@cv}/AU",
          "#{@cv}/Australia",
          "#{@cv}/World"
        ])
        
        doc.parsed_response['result']['visited']['paths'].should eq([
          [ "#{@cv}/World", "#{@cv}/Europe", "#{@cv}/DE" ],
          [ "#{@cv}/World", "#{@cv}/Europe", "#{@cv}/FR" ],
          [ "#{@cv}/World", "#{@cv}/Europe", "#{@cv}/GB" ],
          [ "#{@cv}/World", "#{@cv}/Europe" ],
          [ "#{@cv}/World", "#{@cv}/Asia", "#{@cv}/CN" ],
          [ "#{@cv}/World", "#{@cv}/Asia", "#{@cv}/JP" ],
          [ "#{@cv}/World", "#{@cv}/Asia", "#{@cv}/TW" ],
          [ "#{@cv}/World", "#{@cv}/Asia" ],
          [ "#{@cv}/World", "#{@cv}/America", "#{@cv}/US" ],
          [ "#{@cv}/World", "#{@cv}/America", "#{@cv}/MX" ],
          [ "#{@cv}/World", "#{@cv}/America" ],
          [ "#{@cv}/World", "#{@cv}/Australia", "#{@cv}/AU" ],
          [ "#{@cv}/World", "#{@cv}/Australia" ],
          [ "#{@cv}/World" ]
        ])
      end

################################################################################
## breadth-first, preorder, backward
################################################################################
      
      it "visits nodes in a graph, outbound, breadth-first pre-order, backward" do
        body = "{ \"graphName\" : \"#{@gn}\", \"strategy\" : \"breadthfirst\", \"order\" : \"preorder\", \"itemOrder\" : \"backward\", \"direction\" : \"outbound\", \"startVertex\" : \"#{@cv}/World\", \"visitor\" : \"result.visited.vertices.push(vertex._id); result.visited.paths.push(function() { var paths = [ ]; for (var i = 0; i < path.vertices.length; ++i) { paths.push(path.vertices[i]._id); } return paths;}());\", \"sort\" : \"if (l._key < r._key) { return -1; } else if (l._key > r._key) { return 1; } return 0;\" }"
        doc = ArangoDB.log_post("#{prefix}-visit-breadthfirst-preorder-backward", api, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
       
        doc.parsed_response['result']['visited']['vertices'].should eq([
          "#{@cv}/World",
          "#{@cv}/Australia",
          "#{@cv}/America",
          "#{@cv}/Asia",
          "#{@cv}/Europe",
          "#{@cv}/AU",
          "#{@cv}/MX",
          "#{@cv}/US",
          "#{@cv}/TW",
          "#{@cv}/JP",
          "#{@cv}/CN",
          "#{@cv}/GB",
          "#{@cv}/FR",
          "#{@cv}/DE"
        ])
        
        doc.parsed_response['result']['visited']['paths'].should eq([
          [ "#{@cv}/World" ],
          [ "#{@cv}/World", "#{@cv}/Australia" ],
          [ "#{@cv}/World", "#{@cv}/America" ],
          [ "#{@cv}/World", "#{@cv}/Asia" ],
          [ "#{@cv}/World", "#{@cv}/Europe" ],
          [ "#{@cv}/World", "#{@cv}/Australia", "#{@cv}/AU" ],
          [ "#{@cv}/World", "#{@cv}/America", "#{@cv}/MX" ],
          [ "#{@cv}/World", "#{@cv}/America", "#{@cv}/US" ],
          [ "#{@cv}/World", "#{@cv}/Asia", "#{@cv}/TW" ],
          [ "#{@cv}/World", "#{@cv}/Asia", "#{@cv}/JP" ],
          [ "#{@cv}/World", "#{@cv}/Asia", "#{@cv}/CN" ],
          [ "#{@cv}/World", "#{@cv}/Europe", "#{@cv}/GB" ],
          [ "#{@cv}/World", "#{@cv}/Europe", "#{@cv}/FR" ],
          [ "#{@cv}/World", "#{@cv}/Europe", "#{@cv}/DE" ]
        ])
      end

################################################################################
## breadth-first, postorder, backward
################################################################################
      
      it "visits nodes in a graph, outbound, breadth-first post-order, backward" do
        body = "{ \"graphName\" : \"#{@gn}\", \"strategy\" : \"breadthfirst\", \"order\" : \"postorder\", \"itemOrder\" : \"backward\", \"direction\" : \"outbound\", \"startVertex\" : \"#{@cv}/World\", \"visitor\" : \"result.visited.vertices.push(vertex._id); result.visited.paths.push(function() { var paths = [ ]; for (var i = 0; i < path.vertices.length; ++i) { paths.push(path.vertices[i]._id); } return paths;}());\", \"sort\" : \"if (l._key < r._key) { return -1; } else if (l._key > r._key) { return 1; } return 0;\" }"
        doc = ArangoDB.log_post("#{prefix}-visit-breadthfirst-postorder-backward", api, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
       
        doc.parsed_response['result']['visited']['vertices'].should eq([
          "#{@cv}/AU",
          "#{@cv}/MX",
          "#{@cv}/US",
          "#{@cv}/TW",
          "#{@cv}/JP",
          "#{@cv}/CN",
          "#{@cv}/GB",
          "#{@cv}/FR",
          "#{@cv}/DE",
          "#{@cv}/Australia",
          "#{@cv}/America",
          "#{@cv}/Asia",
          "#{@cv}/Europe",
          "#{@cv}/World"
        ])
        
        doc.parsed_response['result']['visited']['paths'].should eq([
          [ "#{@cv}/World", "#{@cv}/Australia", "#{@cv}/AU" ],
          [ "#{@cv}/World", "#{@cv}/America", "#{@cv}/MX" ],
          [ "#{@cv}/World", "#{@cv}/America", "#{@cv}/US" ],
          [ "#{@cv}/World", "#{@cv}/Asia", "#{@cv}/TW" ],
          [ "#{@cv}/World", "#{@cv}/Asia", "#{@cv}/JP" ],
          [ "#{@cv}/World", "#{@cv}/Asia", "#{@cv}/CN" ],
          [ "#{@cv}/World", "#{@cv}/Europe", "#{@cv}/GB" ],
          [ "#{@cv}/World", "#{@cv}/Europe", "#{@cv}/FR" ],
          [ "#{@cv}/World", "#{@cv}/Europe", "#{@cv}/DE" ],
          [ "#{@cv}/World", "#{@cv}/Australia" ],
          [ "#{@cv}/World", "#{@cv}/America" ],
          [ "#{@cv}/World", "#{@cv}/Asia" ],
          [ "#{@cv}/World", "#{@cv}/Europe" ],
          [ "#{@cv}/World" ]
        ])
      end

################################################################################
## depth-first, minDepth 2 
################################################################################
      
      it "visits nodes in a graph, outbound, minDepth 2" do
        body = "{ \"graphName\" : \"#{@gn}\", \"minDepth\" : 2, \"direction\" : \"outbound\", \"startVertex\" : \"#{@cv}/World\", \"visitor\" : \"result.visited.vertices.push(vertex._id); result.visited.paths.push(function() { var paths = [ ]; for (var i = 0; i < path.vertices.length; ++i) { paths.push(path.vertices[i]._id); } return paths;}());\", \"sort\" : \"if (l._key < r._key) { return -1; } else if (l._key > r._key) { return 1; } return 0;\" }"
        doc = ArangoDB.log_post("#{prefix}-visit-outbound-maxdepth2", api, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
       
        doc.parsed_response['result']['visited']['vertices'].should eq([
          "#{@cv}/DE",
          "#{@cv}/FR",
          "#{@cv}/GB",
          "#{@cv}/CN",
          "#{@cv}/JP",
          "#{@cv}/TW",
          "#{@cv}/US",
          "#{@cv}/MX",
          "#{@cv}/AU"
        ])
        
        doc.parsed_response['result']['visited']['paths'].should eq([
          [ "#{@cv}/World", "#{@cv}/Europe", "#{@cv}/DE" ],
          [ "#{@cv}/World", "#{@cv}/Europe", "#{@cv}/FR" ],
          [ "#{@cv}/World", "#{@cv}/Europe", "#{@cv}/GB" ],
          [ "#{@cv}/World", "#{@cv}/Asia", "#{@cv}/CN" ],
          [ "#{@cv}/World", "#{@cv}/Asia", "#{@cv}/JP" ],
          [ "#{@cv}/World", "#{@cv}/Asia", "#{@cv}/TW" ],
          [ "#{@cv}/World", "#{@cv}/America", "#{@cv}/US" ],
          [ "#{@cv}/World", "#{@cv}/America", "#{@cv}/MX" ],
          [ "#{@cv}/World", "#{@cv}/Australia", "#{@cv}/AU" ]
        ])
      end

################################################################################
## depth-first, maxDepth 0
################################################################################

      it "visits nodes in a graph, outbound, maxDepth 0" do
        body = "{ \"graphName\" : \"#{@gn}\", \"maxDepth\" : 0, \"direction\" : \"outbound\", \"startVertex\" : \"#{@cv}/World\", \"visitor\" : \"result.visited.vertices.push(vertex._id); result.visited.paths.push(function() { var paths = [ ]; for (var i = 0; i < path.vertices.length; ++i) { paths.push(path.vertices[i]._id); } return paths;}());\", \"sort\" : \"if (l._key < r._key) { return -1; } else if (l._key > r._key) { return 1; } return 0;\" }"
        doc = ArangoDB.log_post("#{prefix}-visit-outbound-maxdepth0", api, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
       
        doc.parsed_response['result']['visited']['vertices'].should eq([
          "#{@cv}/World"
        ])
        
        doc.parsed_response['result']['visited']['paths'].should eq([
          [ "#{@cv}/World" ]
        ])
      end

################################################################################
## depth-first, maxDepth 1
################################################################################
      
      it "visits nodes in a graph, outbound, maxDepth 1" do
        body = "{ \"graphName\" : \"#{@gn}\", \"maxDepth\" : 1, \"direction\" : \"outbound\", \"startVertex\" : \"#{@cv}/World\", \"visitor\" : \"result.visited.vertices.push(vertex._id); result.visited.paths.push(function() { var paths = [ ]; for (var i = 0; i < path.vertices.length; ++i) { paths.push(path.vertices[i]._id); } return paths;}());\", \"sort\" : \"if (l._key < r._key) { return -1; } else if (l._key > r._key) { return 1; } return 0;\" }"
        doc = ArangoDB.log_post("#{prefix}-visit-outbound-maxdepth1", api, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
       
        doc.parsed_response['result']['visited']['vertices'].should eq([
          "#{@cv}/World",
          "#{@cv}/Europe",
          "#{@cv}/Asia",
          "#{@cv}/America",
          "#{@cv}/Australia"
        ])
        
        doc.parsed_response['result']['visited']['paths'].should eq([
          [ "#{@cv}/World" ],
          [ "#{@cv}/World", "#{@cv}/Europe" ],
          [ "#{@cv}/World", "#{@cv}/Asia" ],
          [ "#{@cv}/World", "#{@cv}/America" ],
          [ "#{@cv}/World", "#{@cv}/Australia" ]
        ])
      end

################################################################################
## depth-first, using a node without connections
################################################################################
      
      it "visits nodes in a graph, outbound, no connections" do
        body = "{ \"graphName\" : \"#{@gn}\", \"direction\" : \"outbound\", \"startVertex\" : \"#{@cv}/AU\", \"visitor\" : \"result.visited.vertices.push(vertex._id); result.visited.paths.push(function() { var paths = [ ]; for (var i = 0; i < path.vertices.length; ++i) { paths.push(path.vertices[i]._id); } return paths;}());\", \"sort\" : \"if (l._key < r._key) { return -1; } else if (l._key > r._key) { return 1; } return 0;\" }"
        doc = ArangoDB.log_post("#{prefix}-visit-outbound-noconnections", api, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
       
        doc.parsed_response['result']['visited']['vertices'].should eq([
          "#{@cv}/AU"
        ])
        
        doc.parsed_response['result']['visited']['paths'].should eq([
          [ "#{@cv}/AU" ]
        ])
      end

    end

################################################################################
## inbound traversals
################################################################################

    context "inbound traversals" do

################################################################################
## depth-first
################################################################################
      
      it "visits nodes in a graph, inbound" do
        body = "{ \"graphName\" : \"#{@gn}\", \"direction\" : \"inbound\", \"startVertex\" : \"#{@cv}/AU\", \"visitor\" : \"result.visited.vertices.push(vertex._id); result.visited.paths.push(function() { var paths = [ ]; for (var i = 0; i < path.vertices.length; ++i) { paths.push(path.vertices[i]._id); } return paths;}());\", \"sort\" : \"if (l._key < r._key) { return -1; } else if (l._key > r._key) { return 1; } return 0;\" }"
        doc = ArangoDB.log_post("#{prefix}-visit-inbound-simple", api, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
       
        doc.parsed_response['result']['visited']['vertices'].should eq([
          "#{@cv}/AU",
          "#{@cv}/Australia",
          "#{@cv}/World"
        ])
        
        doc.parsed_response['result']['visited']['paths'].should eq([
          [ "#{@cv}/AU" ],
          [ "#{@cv}/AU", "#{@cv}/Australia" ],
          [ "#{@cv}/AU", "#{@cv}/Australia", "#{@cv}/World" ]
        ])
      end

################################################################################
## depth-first, using a node without connections
################################################################################
      
      it "visits nodes in a graph, inbound, no connections" do
        body = "{ \"graphName\" : \"#{@gn}\", \"direction\" : \"inbound\", \"startVertex\" : \"#{@cv}/World\", \"visitor\" : \"result.visited.vertices.push(vertex._id); result.visited.paths.push(function() { var paths = [ ]; for (var i = 0; i < path.vertices.length; ++i) { paths.push(path.vertices[i]._id); } return paths;}());\", \"sort\" : \"if (l._key < r._key) { return -1; } else if (l._key > r._key) { return 1; } return 0;\" }"
        doc = ArangoDB.log_post("#{prefix}-visit-inbound-noconnections", api, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
       
        doc.parsed_response['result']['visited']['vertices'].should eq([
          "#{@cv}/World"
        ])
        
        doc.parsed_response['result']['visited']['paths'].should eq([
          [ "#{@cv}/World" ]
        ])
      end

    end

################################################################################
## using custom functions 
################################################################################

    context "using custom functions" do

################################################################################
## custom filter
################################################################################
      
      it "visits nodes in a graph, own filter 1" do
        body = "{ \"graphName\" : \"#{@gn}\", \"direction\" : \"outbound\", \"filter\" : \"if (vertex._id === '#{@cv}/World') { return 'prune'; }\", \"startVertex\" : \"#{@cv}/World\", \"visitor\" : \"result.visited.vertices.push(vertex._id); result.visited.paths.push(function() { var paths = [ ]; for (var i = 0; i < path.vertices.length; ++i) { paths.push(path.vertices[i]._id); } return paths;}());\", \"sort\" : \"if (l._key < r._key) { return -1; } else if (l._key > r._key) { return 1; } return 0;\" }"
        doc = ArangoDB.log_post("#{prefix}-visit-filter1", api, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
       
        doc.parsed_response['result']['visited']['vertices'].should eq([
          "#{@cv}/World"
        ])
        
        doc.parsed_response['result']['visited']['paths'].should eq([
          [ "#{@cv}/World" ]
        ])
      end

################################################################################
## custom filter
################################################################################
      
      it "visits nodes in a graph, own filter 2" do
        body = "{ \"graphName\" : \"#{@gn}\", \"direction\" : \"outbound\", \"filter\" : \"if (vertex._id === '#{@cv}/Europe' || vertex._id === '#{@cv}/US') { return [ 'prune', 'exclude' ]; }\", \"startVertex\" : \"#{@cv}/World\", \"visitor\" : \"result.visited.vertices.push(vertex._id);\", \"sort\" : \"if (l._key < r._key) { return -1; } else if (l._key > r._key) { return 1; } return 0;\" }"
        doc = ArangoDB.log_post("#{prefix}-visit-filter2", api, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
       
        doc.parsed_response['result']['visited']['vertices'].should eq([
          "#{@cv}/World",
          "#{@cv}/Asia",
          "#{@cv}/CN",
          "#{@cv}/JP",
          "#{@cv}/TW",
          "#{@cv}/America",
          "#{@cv}/MX",
          "#{@cv}/Australia",
          "#{@cv}/AU"
        ])
      end

################################################################################
## custom init
################################################################################
      
      it "visits nodes in a graph, custom init" do
        body = "{ \"graphName\" : \"#{@gn}\", \"direction\" : \"outbound\", \"startVertex\" : \"#{@cv}/World\", \"visitor\" : \"result.myCounter++; result.myVar += 'a';\", \"init\" : \"result.myCounter = 13; result.myVar = 'a'; \", \"sort\" : \"if (l._key < r._key) { return -1; } else if (l._key > r._key) { return 1; } return 0;\" }"
        doc = ArangoDB.log_post("#{prefix}-visit-custom-init", api, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
       
        doc.parsed_response['result']['myCounter'].should eq(27)
        doc.parsed_response['result']['myVar'].should eq("aaaaaaaaaaaaaaa")
      end

################################################################################
## custom expander
################################################################################
      
      it "visits nodes in a graph, own expander" do
        body = "{ \"graphName\" : \"#{@gn}\", \"filter\" : \"if (vertex._id === '#{@cv}/Europe') { return [ 'prune', 'exclude' ]; }\", \"startVertex\" : \"#{@cv}/World\", \"visitor\" : \"result.visited.vertices.push(vertex._id);\", \"expander\" : \"var connections = [ ]; config.datasource.getOutEdges(vertex).forEach(function(c) { connections.push({ vertex: require('internal').db._document(c._to), edge: c }); }); connections = connections.sort( function(l,r) { if (l.edge._key < r.edge._key) { return -1; } else if (l.edge._key > r.edge._key) { return 1; } else { return 0; }}); return connections;\" }"
        doc = ArangoDB.log_post("#{prefix}-visit-expander", api, :body => body)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
       
        doc.parsed_response['result']['visited']['vertices'].should eq([
          "#{@cv}/World",
          "#{@cv}/Asia",
          "#{@cv}/CN",
          "#{@cv}/JP",
          "#{@cv}/TW",
          "#{@cv}/America",
          "#{@cv}/US",
          "#{@cv}/MX",
          "#{@cv}/Australia",
          "#{@cv}/AU"
        ])
      end
      
    end

  end
end
