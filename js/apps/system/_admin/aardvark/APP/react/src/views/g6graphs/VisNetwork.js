import React, { useState, useEffect, useRef } from "react";
import styled from "styled-components";
import { Network } from "vis-network";

const VisNetwork = ({graphData, graphName, options, selectedNode, onSelectNode, onSelectEdge, onDeleteNode, onDeleteEdge, onEditNode, onEditEdge, onSetStartnode, onExpandNode, onAddNodeToDb, onAddEdge}) => {
	const [layoutOptions, setLayoutOptions] = useState(options);
	const [showNodeContextMenu, toggleNodeContextMenu] = useState(false);
	const [showEdgeContextMenu, toggleEdgeContextMenu] = useState(false);
	const [showCanvasContextMenu, toggleCanvasContextMenu] = useState(false);
	const [position, setPosition] = useState({ x: 10, y:10 });
	const [contextMenuNodeID, setContextMenuNodeID] = useState();
	const [contextMenuEdgeID, setContextMenuEdgeID] = useState();
	const [networkData, setNetworkData] = useState();

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
		width: auto;
		background: #eeeeee;
		height: auto;
		z-index: 99999;
		padding: 12px;
		box-shadow: 4px 4px 10px 0px rgba(179,174,174,0.75);
		-webkit-box-shadow: 4px 4px 10px 0px rgba(179,174,174,0.75);
		-moz-box-shadow: 4px 4px 10px 0px rgba(179,174,174,0.75);
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
		//setNetworkData(network);
			// Use network here to configure events, etc
		if(selectedNode && selectedNode.id){
			console.log(selectedNode);
			network.selectNodes([selectedNode.id])
		}

		network.on("selectNode", (event, params) => {
			console.log("event: ", event);
			console.log("params: ", params);
			if (event.nodes.length === 1) {
				console.log("selectNode (event): ", event);
				console.log("Find the node id: ", event.nodes[0]);
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

		/*
		network.on("oncontext", (event) => {
			console.log("ONCONTEXT: ", event);
		});
		*/
		network.on("oncontext", (args) => {
			console.log("args in oncontext: ", args);
			args.event.preventDefault();
			console.log(`network.getEdgeAt: - ${network.getEdgeAt(args.pointer.DOM)}`);
			console.log(`network.getNodeAt: - ${network.getNodeAt(args.pointer.DOM)}`);
			if (network.getNodeAt(args.pointer.DOM)) {
				toggleNodeContextMenu(true);
				network.selectNodes([network.getNodeAt(args.pointer.DOM)]);
				setContextMenuNodeID(network.getNodeAt(args.pointer.DOM));
				setPosition({ left: `${args.pointer.DOM.x}px`, top: `${args.pointer.DOM.y}px` });
			} else if (network.getEdgeAt(args.pointer.DOM)) {
				toggleEdgeContextMenu(true);
				network.selectEdges([network.getEdgeAt(args.pointer.DOM)]);
				setContextMenuEdgeID(network.getEdgeAt(args.pointer.DOM));
				setPosition({ left: `${args.pointer.DOM.x}px`, top: `${args.pointer.DOM.y}px` });
			} else {
				toggleCanvasContextMenu(true);
			}
		});
		network.addEdgeMode();
		console.log("status of network: ", network);
		return () => {
			network.destroy();
		};
		}, [visJsRef, nodes, edges, selectedNode, layoutOptions]);

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
		showNodeContextMenu &&
		<StyledContextComponent {...position}>
			<ul id='graphViewerMenu'>
				<li title='deleteNode'
					onClick={() => {
						onDeleteNode(contextMenuNodeID);
					}}>
					Delete node
				</li>
				<li title='editNode'
					onClick={() => {
						onEditNode(contextMenuNodeID);
					}}>
					Edit node
				</li>
				<li title='expandNode'
					onClick={() => {
						onExpandNode(contextMenuNodeID);
					}}>
					Expand node
				</li>
				<li title='setAsStartnode'
					onClick={() => {
						onSetStartnode(contextMenuNodeID);
					}}>
					Set as startnode
				</li>
					<li title='drawEdge'
						onClick={() => {
							//console.log("network info: ", network);
							//onDrawEdge(contextMenuNodeID);
						}}>
						Draw edge
					</li>
			</ul>
		</StyledContextComponent>
		}
		{
		showEdgeContextMenu &&
		<StyledContextComponent {...position}>
			<ul id='graphViewerMenu'>
				<li title='deleteNode'
					onClick={() => {
						console.log("Edge to delete: ", contextMenuEdgeID);
						onDeleteEdge(contextMenuEdgeID);
					}}>
					Delete edge
				</li>
				<li title='editEdge'
					onClick={() => {
						onEditEdge(contextMenuEdgeID);
					}}>
					Edit edge
				</li>
			</ul>
		</StyledContextComponent>
		}
		{
		showCanvasContextMenu &&
		<StyledContextComponent {...position}>
			<ul id='graphViewerMenu'>
				<li title='addNodeToDb'
					onClick={() => {
						onAddNodeToDb();
					}}>
					Add node to database
				</li>
			</ul>
		</StyledContextComponent>
		}
		<div id="visnetworkdiv" ref={visJsRef} style={{ height: '90vh', width: '100%', background: '#fff' }} />
	</>;
};

export default VisNetwork;