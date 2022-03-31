import React, { useEffect, useState } from "react";
import ReactDOM from "react-dom";
import G6 from "@antv/g6";
import { Tag } from 'antd';
//import { data } from './data';
import { data2 } from './data2';

const G6FuncComponent = ({ data }) => {
  const [graphData, setGraphData] = useState(data);
  const ref = React.useRef(null);
  let graph = null;

  console.log("data in G6FuncComponent: ", data);
  console.log("graphData in G6FuncComponent: ", graphData);

  useEffect(() => {
    console.log("data in G6FuncComponent (useEffect): ", data);
    console.log("graphData in G6FuncComponent (useEffect): ", graphData);
    if (!graph) {
      graph = new G6.Graph({
        container: ReactDOM.findDOMNode(ref.current),
        width: 1200,
        height: 800,
        modes: {
          default: ["drag-canvas"]
        },
        layout: {
          type: "dagre",
          direction: "LR"
        },
        defaultNode: {
          type: "node",
          labelCfg: {
            style: {
              fill: "#000000A6",
              fontSize: 10
            }
          },
          style: {
            stroke: "#72CC4A",
            width: 150
          }
        },
        defaultEdge: {
          type: "polyline"
        }
      });
    }
    setGraphData(data);
    graph.data(graphData);
    graph.render();
    //graph.refresh();
  }, [data]);

  const changeGraph = ({graph}) => {
    graph.data(data2);
    graph.render();
  }

  const printGraph = () => {
    //console.log("Nodes in G6FuncComponent: ", graph.getNodes());
    //console.log("Edges in G6FuncComponent: ", graph.getEdges());
    console.log("graph: ", graph);
  }

/*
<p>
        Nodes: {data.nodes}<br />
        Edges: {data.edges}
      </p>
*/

  return (
    <>
      <Tag color="cyan">{data.nodes.length} data.nodes in G6FuncComponent</Tag>
      <Tag color="magenta">{data.edges.length} data.edges in G6FuncComponent</Tag>
      <Tag color="cyan">{graphData.nodes.length} graphData.nodes in G6FuncComponent</Tag>
      <Tag color="magenta">{graphData.edges.length} graphData.edges in G6FuncComponent</Tag>
      <button onClick={() => changeGraph({graph})}>Change Graph</button>
      <button onClick={() => printGraph()}>Print Graph</button>
      <div ref={ref}></div>
    </>
  );
}

export default G6FuncComponent;
