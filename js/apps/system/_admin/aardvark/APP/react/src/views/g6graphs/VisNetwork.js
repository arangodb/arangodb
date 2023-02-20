import React, { useState, useEffect, useRef } from "react";
import { Network } from "vis-network";

const VisNetwork = ({graphData, graphName, layout, options}) => {
	const [layoutOptions, setLayoutOptions] = useState(layout);

	const nodes = [
		...graphData.nodes
	];

	const edges = [
		...graphData.edges
	];

	const visJsRef = useRef(null);

	useEffect(() => {
		const network =
			visJsRef.current &&
			new Network(visJsRef.current, { nodes, edges }, layout );
			//new Network(visJsRef.current, { nodes, edges }, options );
		// Use `network` here to configure events, etc
		//console.log("network.getViewPosition(): ", network.getViewPosition());
		network.on("selectNode", (event, params) => {
			if (event.nodes.length === 1) {
				console.log("selectNode (event): ", event);
				console.log("selectNode (params): ", params);
			} else {
				console.log("selectNode (event) ELSE: ", event);
				console.log("selectNode (params) ELSE: ", params);
			}
			console.log("NETWOOOORK: ", network.getViewPosition());
		});

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
				/*
				const clickedNode = nodes.get(nodeID);
				console.log("clickedNode: ", clickedNode);
				*/

			  /*
			  clickedNode.color = {
				border: '#000000',
				background: '#000000',
				highlight: {
				  border: '#2B7CE9',
				  background: '#D2E5FF'
				}
			  }
			  nodes.update(clickedNode);
			  */
			}
			//network.redraw();
			//console.log("Is network redrawn?");
		});

		network.on("oncontext", (event) => {
			console.log("ONCONTEXT: ", event);
		});
	}, [visJsRef, nodes, edges, layoutOptions]);

	const changeNodeStyle = () => {
		const nodesTemp = nodes;
		console.log("nodesTemp: ", nodesTemp);
		nodesTemp.forEach((node) => {
			console.log("node id: ", node.id);
			console.log("node shape: ", node.shape);
			node.shape = "circle";
		});
		console.log("nodes: ", nodes);
		console.log("New nodes after chaning dot to circle: ", nodes);
	}






	return <>
		<div id="visnetworkdiv" ref={visJsRef} style={{ height: '90vh', width: '100%', background: '#fff' }} />
	</>;
};

export default VisNetwork;