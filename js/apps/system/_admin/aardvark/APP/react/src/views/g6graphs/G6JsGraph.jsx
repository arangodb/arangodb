/* global arangoHelper, arangoFetch, frontendConfig */

import React, { useEffect, useState, useCallback } from 'react';
import ReactDOM from 'react-dom';
import { data } from './data';
import G6 from '@antv/g6';
import { message, Card, Select } from 'antd';
import { Cell, Grid } from "../../components/pure-css/grid";

const G6JsGraph = () => {
  let [queryString, setQueryString] = useState("/_admin/aardvark/g6graph/routeplanner");
  let [queryMethod, setQueryMethod] = useState("GET");
  let [graphData, setGraphData] = useState(null);
  const ref = React.useRef(null);
  let graph = null;

  // fetching data # start
  const fetchData = useCallback(() => {
    arangoFetch(arangoHelper.databaseUrl(queryString), {
      method: queryMethod,
    })
    .then(response => response.json())
    .then(data => {
      console.log("Loaded data in G6JsGraph: ", data);
      
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
      /*
      graph = new G6.Graph({
        container: ReactDOM.findDOMNode(ref.current),
        width: 1200,
        height: 800,
        modes: {
          default: ['drag-canvas'],
        },
        layout: {
          type: 'dagre',
          direction: 'LR',
        },
        defaultNode: {
          type: 'node',
          labelCfg: {
            style: {
              fill: '#000000A6',
              fontSize: 10,
            },
          },
          style: {
            stroke: '#72CC4A',
            width: 150,
          },
        },
        defaultEdge: {
          type: 'polyline',
        },
      });
      */
    }
    console.log("original data: ", data);
    console.log("graphData: ", graphData);
    graph.data(data);
    //graph.data(graphData);
    graph.render();
  }, []);

  return (
    <div>
        <Card
          title="Pure JS G6 Graph"
          extra={'BLABLA'}
        >
          <div ref={ref}></div>
        </Card>
      </div>
  );
}

export default G6JsGraph;
