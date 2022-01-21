/* global arangoHelper, arangoFetch, frontendConfig */

import React, { useCallback, useEffect, useState } from 'react';
import ReactDOM from 'react-dom';
import { GraphView } from './GraphView';
import { data } from './data';
import { data2 } from './data2';
import G6 from '@antv/g6';
import { Card } from 'antd';
import NodeStyleSelector from './NodeStyleSelector.js';
import EdgeStyleSelector from './EdgeStyleSelector.js';
import AddCollectionNameSelector from './AddCollectionNameSelector.js';
import NodeLabelContentSelector from './NodeLabelContentSelector.js';
import './tooltip.css';

const G6JsGraph = () => {
  const currentUrl = window.location.href;
  const [graphName, setGraphName] = useState(currentUrl.substring(currentUrl.lastIndexOf("/") + 1));
  console.log('graphName: ', graphName);
  let [queryString, setQueryString] = useState(`/_admin/aardvark/g6graph/${graphName}`);
  let [queryMethod, setQueryMethod] = useState("GET");
  let [graphData, setGraphData] = useState(data);
  const ref = React.useRef(null);
  let graph = null;

  // fetching data # start
  const fetchData = useCallback(() => {
    arangoFetch(arangoHelper.databaseUrl(queryString), {
      method: queryMethod,
    })
    .then(response => response.json())
    .then(data => {
      console.log("NEW DATA for graphData: ", data);
      setGraphData(data);
    })
    .catch((err) => {
      console.log(err);
    });
  }, [queryString]);

  useEffect(() => {
    fetchData();
  }, [fetchData]);
  // fetching data # end

  // Instantiate the Minimap
  const minimap = new G6.Minimap({
    size: [100, 100],
    className: 'minimap',
    type: 'delegate',
  });

  // Instantiate the Toolbar
  /*
  const toolbar = new G6.ToolBar({
    position: { x: 10, y: 70 },
  });
  */

  // Instantiate grid
  const grid = new G6.Grid();

  /*
  useEffect(() => {
    if (!graph) {
        graph = new G6.Graph({
          plugins: [minimap], // Configure toolbar and minimap to the graph
          enabledStack: true,
          container: ReactDOM.findDOMNode(ref.current),
          width: 1200,
          height: 400,
          layout: {
            type: 'gForce',
            minMovement: 0.01,
            maxIteration: 100,
            preventOverlap: true,
            damping: 0.99,
            fitView: true,
            linkDistance: 100
          },
          modes: {
            default: [
              'drag-canvas',
              'zoom-canvas',
              'drag-node',
              {
                type: 'tooltip', // Tooltip
                formatText(model) {
                  // The content of tooltip
                  const text = 'label: ' + model.label + ((model.population !== undefined) ? ('<br />population: ' + model.population) : ('<br/> population: No information '));
                  return text;
                },
              },
              {
                type: 'edge-tooltip', // Edge tooltip
                formatText(model) {
                  // The content of the edge tooltip
                  const text =
                    'source: ' +
                    model.source +
                    '<br/> target: ' +
                    model.target +
                    '<br/> type: ' +
                    model.data.type +
                    '<br/> distance: ' +
                    model.data.distance +
                    '<br/> date: ' +
                    model.data.date;
                  return text;
                },
              },
            ], // Allow users to drag canvas, zoom canvas, and drag nodes
          },
          defaultNode: {
            type: 'circle', // 'bubble'
            size: 30,
            labelCfg: {
              position: 'center',
              style: {
                fill: 'blue',
                fontStyle: 'bold',
                fontFamily: 'sans-serif',
                fontSize: 12
              },
            },
          },
          nodeStateStyles: {
          // node style of active state
            active: {
              fillOpacity: 0.8,
            },
          // node style of selected state
            selected: {
              lineWidth: 5,
            },
          },
        });
    }
    graph.data(data);
    //graph.data(graphData);
    graph.render();
  }, []);
  */

  const getNodes = () => {
    const nodes = graph.getNodes();
    console.log("getNodes(): ", nodes);
  }

  const getEdges = () => {
    const edges = graph.getEdges();
    console.log("getEdges(): ", edges);
  }

  const changeGraphData = () => {
    //graph.changeData(data2);
    setGraphData(data2);
  }

  const addNode = () => {
    const nodeModel = {
      id: 'newnode',
      label: 'newnode',
      address: 'cq',
      x: 200,
      y: 150,
      style: {
        fill: 'white',
      },
    };
    
    graph.addItem('node', nodeModel);
  }

  const addEdge = () => {
    const edgeModel = {
      source: 'newnode',
      target: '2',
      data: {
        type: 'name1',
        amount: '1000,00 元',
        date: '2022-01-13'
      }
    };
    
    graph.addItem('edge', edgeModel);
  }

  const updateNodeModel = () => {
    const model = {
      id: '2',
      label: 'node2',
      population: '2,950,000',
      type: 'diamond',
      style: {
        fill: 'red',
      },
    };
    
    // Find the item instance by id
    const item = graph.findById('2');
    graph.updateItem(item, model);
  }

  const updateEdgeModel = () => {
    const model = {
      id: '2',
      label: 'node2',
      distance: '2,950,000',
      type: 'diamond',
      style: {
        fill: 'red',
      },
    };
    
    // Find the item instance by id
    const item = graph.findById('2');
    graph.updateItem(item, model);
  }

  const changeNodeStyle = (typeModel) => {
    graph.node((node) => {
      return {
        id: node.id,
        ...typeModel
      };
    });
    
    graph.data(data);
    graph.render();
  }

  const changeEdgeStyle = (typeModel) => {
    graph.edge((edge) => {
      return {
        id: edge.id,
        ...typeModel
      };
    });

    graph.data(data);
    graph.render();
  }

  const changeNodeLabelContent = (typeModel) => {
    console.log('changeNodeLabelContent (typeModel: ', typeModel);
    graph.node((node) => {
      return {
        id: node.id,
        ...typeModel
      };
    });

    graph.data(data);
    graph.render();
  }

  const changeCollectionName = (typeModel) => {
    console.log('changeCollectionName (typeModel: ', typeModel);
    graph.node((node) => {
      return {
        id: node.id,
        ...typeModel
      };
    });

    graph.data(data);
    graph.render();
  }

  const returnGraph = () => {
    console.log('graph is: ', graph);
  }

  const updateGraphDataNodes = (newNodes) => {
    console.log('graphData is: ', graphData);
    console.log('graphData.nodes are: ', graphData.nodes);
    /*
    const currentEdges = graphData.edges;
    const newGraphData = {
      edges: {
        ...currentEdges
      },
       nodes: {
        ...newNodes
      }
    }
    console.log("currentEdges: ", currentEdges);
    console.log("Complete new created graphData-Object: ", newGraphData);
    */
    setGraphData(newNodes);
    //setGraphData(data2);
  }

  const printGraphData = () => {
    console.log("Current graphData: ", graphData);
  }

  /*
  <NodeStyleSelector onNodeStyleChange={(typeModel) => changeNodeStyle(typeModel)} />
        <EdgeStyleSelector onEdgeStyleChange={(typeModel) => changeEdgeStyle(typeModel)} />
        <AddCollectionNameSelector onAddCollectionNameChange={(typeModel) => changeCollectionName(typeModel)} />
        <NodeLabelContentSelector onNodeLabelContentChange={(typeModel) => changeNodeLabelContent(typeModel)} />
        <button onClick={() => getNodes()}>Get nodes</button>
        <button onClick={() => getEdges()}>Get edges</button>
        <button onClick={() => changeGraphData()}>Change graph data</button>
        <button onClick={() => addNode()}>Add node</button>
        <button onClick={() => addEdge()}>Add edge</button>
        <button onClick={() => updateNodeModel()}>Update node2</button>
        <button onClick={() => returnGraph()}>What is graph?</button>
  */

  return (
    <div>
      <button onClick={() => changeGraphData()}>Change graph data (parent)</button>
      <button onClick={() => printGraphData()}>Print graphData</button>
        <GraphView
            data={graphData}
            onUpdateNodeGraphData={(newGraphData) => updateGraphDataNodes(newGraphData)}
        />
    </div>
  );
}

export default G6JsGraph;
