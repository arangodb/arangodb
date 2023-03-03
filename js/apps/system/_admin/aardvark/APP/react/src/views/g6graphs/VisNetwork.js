import React, { useState, useEffect, useRef } from "react";
import styled from "styled-components";
import { Network } from "vis-network";
import { isEqual } from "lodash";

const VisNetwork = ({graphData, graphName, options, selectedNode, onSelectNode, onSelectEdge, onDeleteNode, onDeleteEdge, onEditNode, onEditEdge, onSetStartnode, onExpandNode, onAddNodeToDb, onAddEdge}) => {
	const [layoutOptions, setLayoutOptions] = useState(options);
	const [contextMenu, toggleContextMenu] = useState("");
	const [position, setPosition] = useState({ x: 10, y:10 });
	const [contextMenuNodeID, setContextMenuNodeID] = useState();
	const [contextMenuEdgeID, setContextMenuEdgeID] = useState();
	const [networkData, setNetworkData] = useState();
	
	const [visGraphData, setVisGraphData] = useState(null);

	if (!isEqual(visGraphData, graphData)) {
		setVisGraphData(graphData);
	};

	const nodes = [
		...graphData.nodes
	];

	const edges = [
		...graphData.edges
	];

	const visJsRef = useRef(null);

	const StyledContextComponent = styled.div`
		position: absolute;
		left: ${(props) => props.left};
		top: ${(props) => props.top};
		display: flex;
		flex-direction: column;
		z-index: 99999;
		background-color: rgb(85, 85, 85);
		color: #ffffff;
		border-radius: 10px;
		box-shadow: 0 5px 15px rgb(85 85 85 / 50%);
		padding: 10px 0;
	`;

	useEffect(() => {
		if(graphData.settings !== undefined) {
			const options = graphData.settings.layout;
			console.log("options: ", options);
			options.manipulation = {
				enabled: false,
				addNode: function (data, callback) {
					// filling in the popup DOM elements
					console.log('add', data);
				},
				editNode: function (data, callback) {
					// filling in the popup DOM elements
					console.log('edit', data);
				},
				addEdge: function (data, callback) {
					console.log('add edge', data);
					onAddEdge(data);
					/*
					if (data.from == data.to) {
						var r = confirm("Do you want to connect the node to itself?");
						if (r === true) {
							callback(data);
						}
					}
					else {
						callback(data);
					}
					*/
					// after each adding you will be back to addEdge mode
					//console.log("network.addEdgeMode(): ", network.addEdgeMode());
					//network.addEdgeMode();

					//network.disableEditMode();
					//network.redraw();
				}
			};
			setLayoutOptions(graphData.settings.layout);
		}

		const network =
			visJsRef.current &&
			new Network(visJsRef.current, { nodes, edges }, layoutOptions );
		setNetworkData(network);
		// Use network here to configure events, etc
		if(selectedNode && selectedNode.id){
			console.log(selectedNode);
			network.selectNodes([selectedNode.id])
		}

		network.on("stabilizationIterationsDone", function () {
			network.setOptions( { physics: false } );
		});

		network.on("selectNode", (event, params) => {
			if (event.nodes.length === 1) {
				onSelectNode(event.nodes[0]);
				console.log("selectNode (params): ", params);
			} else {
				console.log("selectNode (event) ELSE: ", event);
				console.log("selectNode (params) ELSE: ", params);
			}
			console.log("NETWOOOORK: ", network.getViewPosition());
		});

		network.on("selectEdge", (event, params) => {
			if (event.edges.length === 1) {
				onSelectEdge(event.edges[0]);
			}
		});

		network.on("click", function(params) {
			// close all maybe open context menus
			toggleContextMenu("");

			console.log("click (params): ", params);

			/*
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
			*/
			//network.redraw();
			//console.log("Is network redrawn?");
		});

		network.on("oncontext", (args) => {
			args.event.preventDefault();
			const canvasOffset = document.getElementById("visnetworkdiv");
			setPosition({ left: `${args.pointer.DOM.x + canvasOffset.offsetLeft}px`, top: `${args.pointer.DOM.y + canvasOffset.offsetTop}px` });
			if (network.getNodeAt(args.pointer.DOM)) {
				console.log("A node");

				toggleContextMenu("node");
				network.selectNodes([network.getNodeAt(args.pointer.DOM)]);
				setContextMenuNodeID(network.getNodeAt(args.pointer.DOM));
			} else if (network.getEdgeAt(args.pointer.DOM)) {
				console.log("An edge");

				toggleContextMenu("edge");
				network.selectEdges([network.getEdgeAt(args.pointer.DOM)]);
				setContextMenuEdgeID(network.getEdgeAt(args.pointer.DOM));
				//setPosition({ left: `${args.pointer.DOM.x}px`, top: `${args.pointer.DOM.y + canvasOffset.offsetTop}px` });
			} else {
				console.log("The canvas");

				toggleContextMenu("canvas");
			}
		});

		console.log("status of network: ", network);
		return () => {
			network.destroy();
		};
	}, [visJsRef, visGraphData, selectedNode, layoutOptions]);

	/*
	function editEdgeWithoutDrag(data, callback) {
		// filling in the popup DOM elements
		document.getElementById("edge-label").value = data.label;
		document.getElementById("edge-saveButton").onclick = saveEdgeData.bind(
		this,
		data,
		callback
		);
		document.getElementById("edge-cancelButton").onclick =
		cancelEdgeEdit.bind(this, callback);
		document.getElementById("edge-popUp").style.display = "block";
	}
	*/

	return <>
		{
		contextMenu === "node" &&
		<StyledContextComponent {...position}>
			<ul id='graphViewerMenu'>
				<li title='deleteNode'
					onClick={() => {
						onDeleteNode(contextMenuNodeID);
						toggleContextMenu("");
					}}>
					Delete node
				</li>
				<li title='editNode'
					onClick={() => {
						onEditNode(contextMenuNodeID);
						toggleContextMenu("");
					}}>
					Edit node
				</li>
				<li title='expandNode'
					onClick={() => {
						onExpandNode(contextMenuNodeID);
						toggleContextMenu("");
					}}>
					Expand node
				</li>
				<li title='setAsStartnode'
					onClick={() => {
						onSetStartnode(contextMenuNodeID);
						toggleContextMenu("");
					}}>
					Set as startnode
				</li>
			</ul>
		</StyledContextComponent>
		}
		{
		contextMenu === "edge" &&
		<StyledContextComponent {...position}>
			<ul id='graphViewerMenu'>
				<li title='deleteNode'
					onClick={() => {
						onDeleteEdge(contextMenuEdgeID);
						toggleContextMenu("");
					}}>
					Delete edge
				</li>
				<li title='editEdge'
					onClick={() => {
						onEditEdge(contextMenuEdgeID);
						toggleContextMenu("");
					}}>
					Edit edge
				</li>
			</ul>
		</StyledContextComponent>
		}
		{
		contextMenu === "canvas" &&
		<StyledContextComponent {...position}>
			<ul id='graphViewerMenu'>
				<li title='addNodeToDb'
					onClick={() => {
						onAddNodeToDb();
						toggleContextMenu("");
					}}>
					Add node to database
				</li>
				<li title='addEdgeToDb'
					onClick={() => {
						networkData.addEdgeMode();
						toggleContextMenu("");
					}}>
					Add edge to database
				</li>
			</ul>
		</StyledContextComponent>
		}
		<div id="visnetworkdiv" ref={visJsRef} style={{ height: '90vh', width: '100%', background: '#fff' }} />
	</>;
};

export default VisNetwork;