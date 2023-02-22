import React, { useState, useEffect, useRef } from "react";
import styled from "styled-components";
import { Network } from "vis-network";

const VisNetwork = ({graphData, graphName, options, selectedNode, onSelectNode, onSelectEdge, onDeleteNode, onDeleteEdge, onEditNode}) => {
const [layoutOptions, setLayoutOptions] = useState(options);
const [showNodeContextMenu, toggleNodeContextMenu] = useState(false);
const [showEdgeContextMenu, toggleEdgeContextMenu] = useState(false);
const [showContextMenu, toggleContextMenu] = useState(false);
const [position, setPosition] = useState({ x: 10, y:10 });
const [contextMenuNodeID, setContextMenuNodeID] = useState();
const [contextMenuEdgeID, setContextMenuEdgeID] = useState();

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
width: 100px;
background: blue;
height: 100px;
z-index: 99999;
&:hover {
    background: red;
}
`;

useEffect(() => {
if(graphData.settings !== undefined) {
setLayoutOptions(graphData.settings.layout);
}
const network =
visJsRef.current &&
new Network(visJsRef.current, { nodes, edges }, layoutOptions );
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
        toggleContextMenu(false);
    }
});
return () => {
    network.destroy();
};
}, [visJsRef, nodes, edges, selectedNode, layoutOptions]);

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
			<li title='expandNode'>Expand node</li>
			<li title='setAsStartnode'>Set as startnode</li>
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
			<li title='editEdge'>Edit edge</li>
		</ul>
    </StyledContextComponent>
	}
	{
/*
<Menu vertical>
<Menu.Item
className="cursor-pointer"
name="inbox"
onClick={() => {
alert("512");
}}
>
<Label>512</Label>
Spam1
</Menu.Item>
<Menu.Item
className="cursor-pointer"
name="spam"
onClick={() => {
alert("51");
}}
>
<Label>51</Label>
Spam
</Menu.Item>
<Menu.Item
            className="cursor-pointer"
            name="updates z"
            onClick={() => {
                alert("1");
            }}
            >
            <Label>1</Label>
            Updates
            </Menu.Item>
        </Menu>
        */
    }
<div id="visnetworkdiv" ref={visJsRef} style={{ height: '90vh', width: '100%', background: '#fff' }} />
</>;
};

export default VisNetwork;