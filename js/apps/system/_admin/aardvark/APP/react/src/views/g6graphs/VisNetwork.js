import React, { useState, useEffect, useRef } from "react";
import { Network } from "vis-network";

const VisNetwork = ({graphData, graphName, options, selectedNode, onSelectNode, onSelectEdge}) => {
const [layoutOptions, setLayoutOptions] = useState(options);
const nodes = [
    ...graphData.nodes
];

const edges = [
    ...graphData.edges
];

const visJsRef = useRef(null);


useEffect(() => {
    if(graphData.settings !== undefined) {
        setLayoutOptions(graphData.settings.layout);
    }
    const network =
        visJsRef.current &&
        new Network(visJsRef.current, { nodes, edges }, layoutOptions );
    // Use `network` here to configure events, etc

    if(selectedNode && selectedNode.id){
        console.log(selectedNode);
        network.selectNodes([selectedNode.id])
    }

    network.on("selectNode", (event, params) => {
        if (event.nodes.length === 1) {
            onSelectNode(event.nodes[0]);
        }
    });

	network.on("selectEdge", (event, params) => {
		if (event.edges.length === 1) {
			onSelectEdge(event.edges[0]);
		}
    });

    /*
    network.on("click", function(params) {
        console.log("params: ", params);

        var nodeID = params['nodes']['0'];
        console.log("nodeID: ", nodeID);
        if (nodeID) {
            console.log("Do I have the nodes here? ", nodes);
            console.log("typeof nodes: ", typeof nodes);
            console.log("network.body.nodes: ", network.body.nodes);
            console.log("network.body.nodes['worldVertices/world']: ", network.body.nodes['worldVertices/world']);
            console.log("network.body.edges: ", network.body.edges);
            console.log("Object.values(nodes): ", Object.values(nodes));
            const nodesArr = Object.values(nodes);
            console.log("typeof nodesArr: ", typeof nodesArr);
        }
        //network.redraw();
        //console.log("Is network redrawn?");
    });
    */
    return () => {
        network.destroy();
    };
}, [visJsRef, nodes, edges, selectedNode, layoutOptions]);

return <>
	<div id="visnetworkdiv" ref={visJsRef} style={{ height: '90vh', width: '100%', background: '#fff' }} />
	</>;
};

export default VisNetwork;
