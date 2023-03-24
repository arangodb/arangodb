import React, { useState, useEffect, useRef } from "react";
import styled from "styled-components";
import { Network } from "vis-network";
import { isEqual } from "lodash";
import { ProgressBar } from "./ProgressBar";
import { Alert, AlertIcon, Button, Box, Center, Progress, Stack, Text } from '@chakra-ui/react';

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

const StyledEditModeInfoComponent = styled.div`
	position: absolute;
	left: ${(props) => props.left};
	top: ${(props) => props.top};
	z-index: 99999;
`;

const VisNetwork = ({graphData, graphName, options, selectedNode, onSelectNode, onSelectEdge, onDeleteNode, onDeleteEdge, onEditNode, onEditEdge, onSetStartnode, onExpandNode, onAddNodeToDb, onAddEdge}) => {
	const [layoutOptions, setLayoutOptions] = useState(options);
	const [contextMenu, toggleContextMenu] = useState("");
	const [position, setPosition] = useState({ x: 10, y:10 });
	const [infoPosition, setInfoPosition] = useState({ x: 10, y:10 });
	const [showInfo, setShowInfo] = useState(false);
	const [contextMenuNodeID, setContextMenuNodeID] = useState();
	const [contextMenuEdgeID, setContextMenuEdgeID] = useState();
	const [networkData, setNetworkData] = useState();
	const [progressValue, setProgressValue] = useState(0);
	
	const [visGraphData, setVisGraphData] = useState(null);
	const [editEdgeMode, setEditEdgeMode] = useState(false);

	if (!isEqual(visGraphData, graphData)) {
		setVisGraphData(graphData);
	};

	useEffect(() => {
		if(networkData) {
			if(editEdgeMode) {
				networkData.addEdgeMode();
				const canvasOffset = document.getElementById("visnetworkdiv");
				if (canvasOffset) {
					setInfoPosition({ left: `${canvasOffset.offsetLeft}px`, top: `${canvasOffset.offsetTop}px` });
				}
				setShowInfo(true);
			} else {
				setShowInfo(false);
				networkData.disableEditMode();
			}
		}
	}, [editEdgeMode]);

	const nodes = [
		...graphData.nodes
	];

	const edges = [
		...graphData.edges
	];

	const visJsRef = useRef(null);

	useEffect(() => {
		if(graphData.settings !== undefined) {
			const options = graphData.settings.layout;
			options.manipulation = {
				enabled: false,
				addEdge: function (data, callback) {
					onAddEdge(data);
					setEditEdgeMode(false);
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
			network.selectNodes([selectedNode.id])
		}

		network.on("stabilizationProgress", function (params) {
			if(nodes.length > 1) {
				const widthFactor = params.iterations / params.total;
				const calculatedProgressValue = Math.round(widthFactor * 100);
				setProgressValue(calculatedProgressValue);
			}
		});

		network.on("stabilizationIterationsDone", function () {
			network.setOptions( { physics: false } );
		});

		network.on("stabilized", function () {
			setProgressValue(100);
		});

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

		network.on("click", function(params) {
			// close all maybe open context menus
			toggleContextMenu("");
		});

		network.on("oncontext", (args) => {
			args.event.preventDefault();
			const canvasOffset = document.getElementById("visnetworkdiv");
			if (canvasOffset) {
				setPosition({ left: `${args.pointer.DOM.x + canvasOffset.offsetLeft}px`, top: `${args.pointer.DOM.y + canvasOffset.offsetTop}px` });
			}
			if (network.getNodeAt(args.pointer.DOM)) {
				toggleContextMenu("node");
				network.selectNodes([network.getNodeAt(args.pointer.DOM)]);
				setContextMenuNodeID(network.getNodeAt(args.pointer.DOM));
			} else if (network.getEdgeAt(args.pointer.DOM)) {

				toggleContextMenu("edge");
				network.selectEdges([network.getEdgeAt(args.pointer.DOM)]);
				setContextMenuEdgeID(network.getEdgeAt(args.pointer.DOM));
			} else {
				toggleContextMenu("canvas");
			}
		});

		return () => {
			network.destroy();
		};
	}, [visJsRef, visGraphData, selectedNode, layoutOptions]);

	return <>
		{
			progressValue < 100 &&
			<Center>
				<Box bg='white' w='100%' p={5} m={4}>
					<Progress colorScheme='green' size='lg' hasStripe value={progressValue} />
				</Box>
			</Center>
		}
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
						setEditEdgeMode(true);
						toggleContextMenu("");
					}}>
					Add edge to database
				</li>
			</ul>
		</StyledContextComponent>
		}
		{
		showInfo &&
		<StyledEditModeInfoComponent {...infoPosition}>
			<Alert status='warning'>
				<AlertIcon />
				<Stack spacing={4} direction='row' align='center'>
					<Text fontSize='md'>&quot;Add edge mode&quot; is on: Click a node and drag the edge to the end node</Text>
					<Button onClick={() => setEditEdgeMode(false)} colorScheme='blue'>Turn off</Button>
				</Stack>
			</Alert>
		</StyledEditModeInfoComponent>
		}
		<ProgressBar />
		<div id="visnetworkdiv" ref={visJsRef} style={{ height: '90vh', width: '97%', background: '#fff', margin: 'auto' }} />
	</>;
};

export default VisNetwork;