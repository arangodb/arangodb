import React, { useEffect, useState } from 'react';
import ReactDOM from 'react-dom';
import G6 from '@antv/g6';

const HorizonGraph = () => {
  const ref = React.useRef(null);
  let graph = null;

  useEffect(() => {
    const toolbar = new G6.ToolBar({
      position: { x: 10, y: 10 },
    });
    if (!graph) {
      graph = new G6.Graph({
        container: ReactDOM.findDOMNode(ref.current),
        width: 1200,
        height: 800,
        plugins: [toolbar],
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
    }

    const initData = {
      nodes: [
        { id: 'node0' }, { id: 'node1' }, { id: 'node2' },
        { id: 'node3' }, { id: 'node4' }
      ],
      edges: [
        { source: 'node0', target: 'node1' },
        { source: 'node0', target: 'node2' },
        { source: 'node0', target: 'node3' },
        { source: 'node0', target: 'node4' }
      ],
    };

    graph.data(initData);
    graph.render();
  }, []);

  return <div ref={ref}></div>;
}

export default HorizonGraph;
