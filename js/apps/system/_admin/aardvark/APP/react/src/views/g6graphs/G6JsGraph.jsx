/* global arangoHelper, arangoFetch, frontendConfig */

import React, { useEffect, useState } from 'react';
import ReactDOM from 'react-dom';
import { data } from './data';
import { data2 } from './data2';
import G6 from '@antv/g6';
import { Card } from 'antd';
import NodeStyleSelector from './NodeStyleSelector.js';
import EdgeStyleSelector from './EdgeStyleSelector.js';
import './tooltip.css';

const G6JsGraph = () => {
  let [graphData, setGraphData] = useState(null);
  const ref = React.useRef(null);
  let graph = null;

  // Instantiate the Minimap
  const minimap = new G6.Minimap({
    size: [100, 100],
    className: 'minimap',
    type: 'delegate',
  });

  // Instantiate grid
  const grid = new G6.Grid();

  useEffect(() => {
    if (!graph) {
        graph = new G6.Graph({
          plugins: [minimap], // Configure minimap to the graph
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
    graph.render();
  }, [graphData]);

  const getNodes = () => {
    const nodes = graph.getNodes();
    console.log("getNodes(): ", nodes);
  }

  const getEdges = () => {
    const edges = graph.getEdges();
    console.log("getEdges(): ", edges);
  }

  const changeGraphData = () => {
    graph.changeData(data2);
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

  return (
    <div>
        <NodeStyleSelector onNodeStyleChange={(typeModel) => changeNodeStyle(typeModel)} />
        <EdgeStyleSelector onEdgeStyleChange={(typeModel) => changeEdgeStyle(typeModel)} />
        <button onClick={() => getNodes()}>Get nodes</button>
        <button onClick={() => getEdges()}>Get edges</button>
        <button onClick={() => changeGraphData()}>Change graph data</button>
        <button onClick={() => addNode()}>Add node</button>
        <button onClick={() => addEdge()}>Add edge</button>
        <button onClick={() => updateNodeModel()}>Update node2</button>
        <Card
          title="Pure JS G6 Graph"
        >
          <div ref={ref}></div>
        </Card>
      </div>
  );
}

export default G6JsGraph;
