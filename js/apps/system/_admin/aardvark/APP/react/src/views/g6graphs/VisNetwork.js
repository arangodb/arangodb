import React, { useState, useEffect, useRef } from "react";
import { Network } from "vis-network";

const VisNetwork = ({graphData, graphName, options}) => {
	console.log("graphData inVisNetwork: ", graphData);
	console.log("graphData.nodes inVisNetwork: ", graphData.nodes);
	console.log("graphData.edges inVisNetwork: ", graphData.edges);
	console.log("graphName inVisNetwork: ", graphName);
	console.log("options inVisNetwork: ", options);
	const [layoutOptions, setLayoutOptions] = useState(options);

	const nodes = [
		...graphData.nodes
	];

	const edges = [
		...graphData.edges
	];

	const visJsRef = useRef(null);

	useEffect(() => {
		console.log("useEffect in VisNetwork.js triggered");
		console.log("options in useEffect: ", options);
		const network =
			visJsRef.current &&
			new Network(visJsRef.current, { nodes, edges }, layoutOptions );
			//new Network(visJsRef.current, { nodes, edges }, options );
		// Use `network` here to configure events, etc
		//console.log("network.getViewPosition(): ", network.getViewPosition());
		console.log("typeof network: ", typeof network);
		console.log("network: ", network);
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
			/*
		  const idSplit = node._cfg.id.split('/');
	
		  const model = {
			label: `${idSplit[1]} (${idSplit[0]})`
		  };
		  this.graph.updateItem(node, model);
		  */
		});
		console.log("nodes: ", nodes);
		console.log("New nodes after chaning dot to circle: ", nodes);
	}

	const interactionOptions = {
		dragNodes: true,
		dragView: true,
		hideEdgesOnDrag: true,
		hideNodesOnDrag: false,
		hover: true,
		hoverConnectedEdges: true,
		keyboard: {
		  enabled: false,
		  speed: {x: 10, y: 10, zoom: 0.02},
		  bindToWindow: true
		},
		multiselect: false,
		navigationButtons: true,
		selectable: true,
		selectConnectedEdges: true,
		tooltipDelay: 300,
		zoomSpeed: 0.25,
		zoomView: true
	};


    const barnesHutOptions = {
        interaction: interactionOptions,
        layout: {
            hierarchical: false
        },
        physics: {
            barnesHut: {
                gravitationalConstant: -2250,
                centralGravity: 0.4,
                springLength: 76,
                damping: 0.095
            },
            solver: "barnesHut"
        }
        /*
        "edges": {
            "smooth": {
            "type": "continuous",
            "forceDirection": "none"
            }
        },
        */
    };

    const forceAtlas2BasedOptions = {
		interaction: interactionOptions,
        "edges": {
            "smooth": {
                "type": "continuous",
                "forceDirection": "none"
            }
        },
        layout: {
            hierarchical: false
        },
        "physics": {
            "forceAtlas2Based": {
                "springLength": 100
            },
            "minVelocity": 0.75,
            "solver": "forceAtlas2Based"
        }
    };

    const circleOptions = {
        interaction: interactionOptions,
        layout: {
            hierarchical: false
        },
        physics: {
            barnesHut: {
                gravitationalConstant: -2250,
                centralGravity: 0.4,
                springLength: 76,
                damping: 0.095
            },
            solver: "barnesHut"
        },
        nodes: {
            shape: 'circle'
        }
    };

    const repulsionOptions = {
		interaction: interactionOptions,
        layout: {
            hierarchical: false
        },
        physics: {
            /*
            barnesHut: {
                enabled: false
            },
            */
            repulsion: {
                nodeDistance: 109,
                centralGravity: 0.1,
                springLength: 193,
                springConstant: 0.067,
                damping: 0.1
            }
        }
        //smoothCurves: [object Object]
        /*
        "edges": {
            "smooth": {
            "type": "continuous",
            "forceDirection": "none"
            }
        },
        "physics": {
            "minVelocity": 0.75,
            "solver": "repulsion"
        }
        */
    };

    const hierarchicalRepulsionOptions = {
		interaction: interactionOptions,
        layout: {
            hierarchical: false
        },
        "edges": {
            "smooth": {
            "type": "continuous",
            "forceDirection": "none"
            }
        },
        "physics": {
            "hierarchicalRepulsion": {
                "centralGravity": 0,
                "avoidOverlap": 1
            },
            "minVelocity": 0.75,
            "solver": "hierarchicalRepulsion"
        }
    }

    const hierarchicalOptions = {
		interaction: interactionOptions,
        layout: {
            hierarchical: {
                levelSeparation: 150,
                nodeSpacing: 150,
                direction: "UD"
            },
        },
        edges: {
            smooth: false
        },
        physics: {
            barnesHut: {
                gravitationalConstant: -2250,
                centralGravity: 0.4,
                springLength: 76,
                damping: 0.095
            },
            solver: "barnesHut"
        }
    };

	return <>
		<button onClick={() => {
			console.log("barnesHutOptions: ", barnesHutOptions);
			setLayoutOptions(barnesHutOptions);
		}}
		>barnesHutOptions</button>

		<button onClick={() => {
			console.log("forceAtlas2BasedOptions: ", forceAtlas2BasedOptions);
			setLayoutOptions(forceAtlas2BasedOptions);
		}}
		>forceAtlas2BasedOptions</button>

		<button onClick={() => {
			console.log("circleOptions: ", circleOptions);
			setLayoutOptions(circleOptions);
		}}
		>circleOptions</button>

		<button onClick={() => {
			console.log("repulsionOptions: ", repulsionOptions);
			setLayoutOptions(repulsionOptions);
		}}
		>repulsionOptions</button>

		<button onClick={() => {
			console.log("hierarchicalRepulsionOptions: ", hierarchicalRepulsionOptions);
			setLayoutOptions(hierarchicalRepulsionOptions);
		}}
		>hierarchicalRepulsionOptions</button>

		<button onClick={() => {
			console.log("hierarchicalOptions: ", hierarchicalOptions);
			setLayoutOptions(hierarchicalOptions);
		}}
		>hierarchicalOptions</button>

		<hr />

		<button onClick={() => {
			console.log("Current layoutOptions: ", layoutOptions);
		}}
		>Current layoutOptions</button>

		<button onClick={() => {
			changeNodeStyle();
			console.log("changeNodeStyle");
		}}
		>changeNodeStyle</button>

		<button onClick={() => {
			console.log("nodes: ", nodes);
		}}
		>print nodes</button>

		<div ref={visJsRef} style={{ height: '90vh', width: '100%', background: "#fff" }} />
	</>;
};

/*
const VisNetwork = () => {
	// Create a ref to provide DOM access
	const visJsRef = useRef(null);
	useEffect(() => {
		// Once the ref is created, we'll be able to use vis
	}, [visJsRef]);
	return <div ref={visJsRef} />;
};
*/

export default VisNetwork;