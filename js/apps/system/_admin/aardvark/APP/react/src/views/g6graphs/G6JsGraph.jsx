/* global arangoHelper, arangoFetch, frontendConfig */

import React, { useEffect, useState } from 'react';
import ReactDOM from 'react-dom';
import { data } from './data';
import { data2 } from './data2';
import G6 from '@antv/g6';
import { Card } from 'antd';

const G6JsGraph = () => {
  let [graphData, setGraphData] = useState(null);
  const ref = React.useRef(null);
  let graph = null;

  useEffect(() => {
    if (!graph) {
        graph = new G6.Graph({
          container: ReactDOM.findDOMNode(ref.current),
          width: 1200,
          height: 800,
          layout: {
            type: 'gForce',
            //minMovement: 0.01,
            //maxIteration: 5000,
            preventOverlap: true,
            damping: 0.99,
            fitView: true,
            linkDistance: 100
          },
          modes: {
            default: ['drag-canvas', 'zoom-canvas', 'drag-node'], // Allow users to drag canvas, zoom canvas, and drag nodes
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

  return (
    <div>
        <button onClick={() => getNodes()}>Get nodes</button>
        <button onClick={() => getEdges()}>Get edges</button>
        <Card
          title="Pure JS G6 Graph"
        >
          <div ref={ref}></div>
        </Card>
      </div>
  );
}

export default G6JsGraph;
