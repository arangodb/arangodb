import React, { useState, useEffect, useRef } from "react";
import { Network } from "vis-network";

const VisNetwork = ({graphData, graphName, options}) => {
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
	}, [visJsRef, nodes, edges, layoutOptions]);
	
	return <>
		<div id="visnetworkdiv" ref={visJsRef} style={{ height: '90vh', width: '100%', background: '#fff' }} />
	</>;
};

export default VisNetwork;