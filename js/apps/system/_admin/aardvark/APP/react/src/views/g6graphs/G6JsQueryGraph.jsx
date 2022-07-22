/* global arangoHelper, arangoFetch, $ */

import React, { useCallback, useEffect, useState } from 'react';
import { QueryGraphView } from './QueryGraphView';
import { AttributesInfo } from './AttributesInfo';
import { data } from './data';
import { EditNodeModal } from './EditNodeModal';
import { EditEdgeModal } from './EditEdgeModal';
import { DeleteEdgeModal } from './DeleteEdgeModal';
import { AddNodeModal } from './AddNodeModal';
import { AddEdgeModal } from './AddEdgeModal';
import { FetchFullGraphModal } from './FetchFullGraphModal';
import { omit, pick, uniqBy } from "lodash";
import { UrlParametersContext } from "./url-parameters-context";
import URLPARAMETERS from "./UrlParameters";
import ButtonScrollTo from "./ButtonScrollTo";
import './tooltip.css';

const G6JsQueryGraph = (result) => {

  console.log("In G6JsQueryGraph (result): ", result);
  console.log("In G6JsQueryGraph (result.result): ", result.result);
  console.log("In G6JsQueryGraph (result.result.modified): ", result.result.modified);
  console.log("In G6JsQueryGraph (result.result.modified.nodes): ", result.result.modified.nodes);
  console.log("In G6JsQueryGraph (result.result.modified.edges): ", result.result.modified.edges);

  console.log("############# In G6JsQueryGraph (result.result.g6Nodes): ", result.result.g6Nodes);
  console.log("############# In G6JsQueryGraph (result.result.g6Edges): ", result.result.g6Edges);

  console.log("############# In G6JsQueryGraph (result.result.g6Result): ", result.result.g6Result);

  let [graphData, setGraphData] = useState(result.result.g6Result);
  const [lookedUpData, setLookedUpData] = useState([]);
  //console.log("In G6JsQueryGraph (result.modified): ", result.modified);
  //console.log("In G6JsQueryGraph (result.modified.nodes): ", result.modified.nodes);
  //console.log("In G6JsQueryGraph (result.modified.edges): ", result.modified.edges);
  /*
  const currentUrl = window.location.href;
  const [graphName, setGraphName] = useState(currentUrl.substring(currentUrl.lastIndexOf("/") + 1));
  let localUrlParameters = URLPARAMETERS;
  const settingsKey = `${graphName}-gv-urlparameters`;
  if(localStorage.getItem(settingsKey) !== null) {
    localUrlParameters = JSON.parse(localStorage.getItem(settingsKey));
  }
  const [urlParameters, setUrlParameters] = React.useState(localUrlParameters);

  let responseTimesObject = {
    fetchStarted: null,
    fetchFinished: null,
    fetchDuration: null
  }

  let urlParamsObject = {
    depth: 1,
    limit: 1,
    nodeColorByCollection: true,
    edgeColorByCollection: false,
    nodeColor: "#2ecc71",
    nodeColorAttribute: '',
    edgeColor: '#cccccc',
    edgeColorAttribute: '',
    nodeLabel: 'key',
    edgeLabel: '',
    nodeSize: '',
    nodeSizeByEdges: true,
    edgeEditable: true,
    nodeLabelByCollection: false,
    edgeLabelByCollection: false,
    nodeStart: '',
    barnesHutOptimize: true,
    query: '',
    mode: 'all'
  };

  const [urlParams, setUrlParams] = useState(urlParamsObject);
  const [lookedUpData, setLookedUpData] = useState([]);
  const [nodesColorAttributes, setNodesColorAttributes] = useState({});
  const [edgesColorAttributes, setEdgesColorAttributes] = useState({});
  const [responseTimes, setResponseTimes] = useState(responseTimesObject);
  let [queryString, setQueryString] = useState(`/_admin/aardvark/g6graph/${graphName}`);
  let [queryMethod, setQueryMethod] = useState("GET");
  let [graphData, setGraphData] = useState(data);
  const [showFetchFullGraphModal, setShowFetchFullGraphModal] = useState(false);
  const [showEditNodeModal, setShowEditNodeModal] = useState(false);
  const [showEditEdgeModal, setShowEditEdgeModal] = useState(false);
  const [showDeleteEdgeModal, setShowDeleteEdgeModal] = useState(false);
  const [vertexCollections, setVertexCollections] = useState([]);
  const [vertexCollectionsColors, setVertexCollectionsColors] = useState();
  const [nodesSizeMinMax, setNodesSizeMinMax] = useState();
  const [connectionsMinMax, setConnectionsMinMax] = useState();
  const [edgeCollections, setEdgeCollections] = useState([]);
  const [edgeModelToAdd, setEdgeModelToAdd] = useState({});
  const [nodeToEdit, setNodeToEdit] = useState({});
  const [edgeToEdit, setEdgeToEdit] = useState({});
  const [edgeToDelete, setEdgeToDelete] = useState({});
  const [nodeDataToEdit, setNodeDataToEdit] = useState();
  const [basicNodeDataToEdit, setBasicNodeDataToEdit] = useState([]);
  const [edgeDataToEdit, setEdgeDataToEdit] = useState();
  const [edgeDataToDelete, setEdgeDataToDelete] = useState();
  const [basicEdgeDataToEdit, setBasicEdgeDataToEdit] = useState([]);
  const [basicEdgeDataToDelete, setBasicEdgeDataToDelete] = useState([]);
  const [nodeKey, setNodeKey] = useState('');
  const [edgeKey, setEdgeKey] = useState('');
  const [edgeToDeleteKey, setEdgeToDeleteKey] = useState('');
  const [nodeCollection, setNodeCollection] = useState('');
  const [edgeCollection, setEdgeCollection] = useState('');
  const [edgeToDeleteCollection, setEdgeToDeleteCollection] = useState('');
  const [nodeToAdd, setNodeToAdd] = useState();
  const [isNewNodeToEdit, setIsNewNodeToEdit] = useState(false);
  const [showNodeToAddModal, setShowNodeToAddModal] = useState();
  const [showEdgeToAddModal, setShowEdgeToAddModal] = useState();

  const randomColor = () => {
    return Math.floor(Math.random()*16777215).toString(16);
  }

  const fetchData = useCallback(() => {
    responseTimesObject.fetchStarted = new Date();
    $.ajax({
      type: queryMethod,
      url: arangoHelper.databaseUrl(queryString),
      contentType: 'application/json',
      data: urlParameters,
      success: function (data) {
        responseTimesObject.fetchFinished = new Date();
        responseTimesObject.fetchDuration = Math.abs(responseTimesObject.fetchFinished.getTime() - responseTimesObject.fetchStarted.getTime());
        setResponseTimes(responseTimesObject);
        setVertexCollections(data.settings.vertexCollections);
        if(data.settings.connectionsMinMax) {
          setConnectionsMinMax(data.settings.connectionsMinMax);
        }
        if(data.settings.nodesColorAttributes) {
          setNodesColorAttributes(data.settings.nodesColorAttributes);
        }
        if(data.settings.nodesSizeMinMax) {
          setNodesSizeMinMax(data.settings.nodesSizeMinMax);
        }
        if(data.settings.edgesColorAttributes) {
          setEdgesColorAttributes(data.settings.edgesColorAttributes);
        }
        const collectionColors = [];
        Object.keys(data.settings.vertexCollections)
        .map((key, i) => {
          collectionColors[data.settings.vertexCollections[key].name] = "#" + randomColor();
          return true;
        });
        setVertexCollectionsColors(collectionColors);
        setEdgeCollections(data.settings.edgeCollections);
        setGraphData(data);
      },
      error: function (e) {
        arangoHelper.arangoError('Graph', 'Could not load graph.');
        console.log(e);
      }
    });
  }, [queryString]);

  useEffect(() => {
    fetchData();
  }, [fetchData]);

  const fetchEdgeCollections = useCallback(() => {
    arangoFetch(arangoHelper.databaseUrl('/_api/collection'), {
      method: "GET"
    })
    .then(response => response.json())
    .then(data => {
      const returnedEdgeCollections = data.result.filter(collection => collection.type === 3);
      setEdgeCollections(returnedEdgeCollections);
    })
    .catch((err) => {
      arangoHelper.arangoError('Graph', 'Could not load collection.');
      console.log(err);
    });
  }, []);

  useEffect(() => {
    fetchEdgeCollections();
  }, [fetchEdgeCollections]);

  const updateGraphDataWithNode = (newNode) => {
    const currentEdges = graphData.edges;
    const newGraphData = {
      nodes: [
        ...graphData.nodes,
        newNode
      ],
      edges: [
        ...currentEdges
      ]
    };
    setGraphData(newGraphData);
  }

  const updateGraphDataWithEdge = (newEdge) => {
    const currentNodes = graphData.nodes;
    const newGraphData = {
      nodes: [
        ...currentNodes,
      ],
      edges: [
        ...graphData.edges,
        newEdge
      ]
    };
    setGraphData(newGraphData);
  }

  const updateGraphDataNodes = (newNodes) => {
    const currentEdges = graphData.edges;
    const newGraphData = {
      nodes: [
        ...newNodes
      ],
      edges: [
        ...currentEdges
      ]
    };
    setGraphData(newGraphData);
  }

  const updateGraphDataEdges = (newEdges) => {
    const currentNodes = graphData.nodes;
    const newGraphData = {
      nodes: [
        ...currentNodes
      ],
      edges: [
        ...newEdges
      ]
    };
    setGraphData(newGraphData);
  }

  const removeNode = (nodeId) => {
    const nodes = graphData.nodes;
    const updatedNodes = nodes.filter(item => item.id !== nodeId);
    const currentEdges = graphData.edges;
    const newGraphData = {
      nodes: [
        ...updatedNodes
      ],
      edges: [
        ...currentEdges
      ]
    };
    setGraphData(newGraphData);
  }

  const removeEdge = (edgeId) => {
    const edges = graphData.edges;
    const updatedEdges = edges.filter(item => item.id !== edgeId);
    const currentNodes = graphData.nodes;
    const newGraphData = {
      nodes: [
        ...currentNodes
      ],
      edges: [
        ...updatedEdges
      ]
    };
    setGraphData(newGraphData);
  }

  const openEditNodeModal = (node) => {
    const slashPos = node.indexOf("/");
    setNodeToEdit(node);
    setNodeKey(node.substring(slashPos + 1));
    setNodeCollection(node.substring(0, slashPos));
    const nodeDataObject = {
      "keys": [
        node.substring(slashPos + 1)
      ],
      "collection": node.substring(0, slashPos)
    };

    arangoFetch(arangoHelper.databaseUrl("/_api/simple/lookup-by-keys"), {
      method: "PUT",
      body: JSON.stringify(nodeDataObject),
    })
    .then(response => response.json())
    .then(data => {
      const allowedDocuments = omit(data.documents[0], '_id', '_key', '_rev');
      setNodeDataToEdit(allowedDocuments);
      setBasicNodeDataToEdit(pick(data.documents[0], ['_id', '_key', '_rev']));
    })
    .catch((error) => {
      arangoHelper.arangoError('Graph', 'Could not look up this node.');
      setShowEditNodeModal(false);
    });

    setIsNewNodeToEdit(true);
    setShowEditNodeModal(true);
  }

  const openDeleteEdgeModal = (edge) => {
    const slashPos = edge.indexOf("/");
    setEdgeToDelete(edge);
    setEdgeToDeleteKey(edge.substring(slashPos + 1));
    setEdgeToDeleteCollection(edge.substring(0, slashPos));
    const edgeDataObject = {
      "keys": [
        edge.substring(slashPos + 1)
      ],
      "collection": edge.substring(0, slashPos)
    };

    arangoFetch(arangoHelper.databaseUrl("/_api/simple/lookup-by-keys"), {
      method: "PUT",
      body: JSON.stringify(edgeDataObject),
    })
    .then(response => response.json())
    .then(data => {
      const allowedDocuments = omit(data.documents[0], '_id', '_key', '_rev', '_from', '_to');
      setBasicEdgeDataToDelete(pick(data.documents[0], ['_id', '_key', '_rev', '_from', '_to']));
      setEdgeDataToDelete(allowedDocuments);
    })
    .catch((err) => {
      arangoHelper.arangoError('Graph', 'Could not look up this edge.');
      console.log(err);
    });
    setShowDeleteEdgeModal(true);
  }

  const openEditEdgeModal = (edge) => {
    const slashPos = edge.indexOf("/");
    setEdgeToEdit(edge);
    setEdgeKey(edge.substring(slashPos + 1));
    setEdgeCollection(edge.substring(0, slashPos));
    const edgeDataObject = {
      "keys": [
        edge.substring(slashPos + 1)
      ],
      "collection": edge.substring(0, slashPos)
    };

    arangoFetch(arangoHelper.databaseUrl("/_api/simple/lookup-by-keys"), {
      method: "PUT",
      body: JSON.stringify(edgeDataObject),
    })
    .then(response => response.json())
    .then(data => {
      const allowedDocuments = omit(data.documents[0], '_id', '_key', '_rev', '_from', '_to');
      setBasicEdgeDataToEdit(pick(data.documents[0], ['_id', '_key', '_rev', '_from', '_to']));
      setEdgeDataToEdit(allowedDocuments);
    })
    .catch((err) => {
      arangoHelper.arangoError('Graph', 'Could not look up this edge.');
      console.log(err);
    });
    setShowEditEdgeModal(true);
  }

  const openAddEdgeModal = (newEdge) => {
    setShowEdgeToAddModal(true);
  }

  const openAddNodeModal = () => {
    setNodeToAdd({});
    setShowNodeToAddModal(true);
  }

  const expandNode = (node) => {
    const url = `/_admin/aardvark/g6graph/${graphName}?depth=${urlParameters.depth}&limit=${urlParameters.limit}&nodeColor=%23${urlParameters.nodeColor}&nodeColorAttribute=${urlParameters.nodeColorAttribute}&nodeColorByCollection=${urlParameters.nodeColorByCollection}&edgeColor=%23${urlParameters.edgeColor}&edgeColorAttribute=${urlParameters.edgeColorAttribute}&edgeColorByCollection=${urlParameters.edgeColorByCollection}&nodeLabel=${urlParameters.nodeLabel}&edgeLabel=${urlParameters.edgeLabel}&nodeSize=${urlParameters.nodeSize}&nodeSizeByEdges=${urlParameters.nodeSizeByEdges}&edgeEditable=${urlParameters.edgeEditable}&nodeLabelByCollection=${urlParameters.nodeLabelByCollection}&edgeLabelByCollection=${urlParameters.edgeLabelByCollection}&nodeStart=${urlParameters.nodeStart}&barnesHutOptimize=${urlParameters.barnesHutOptimize}&query=FOR v, e, p IN 1..1 ANY "${node}" GRAPH "${graphName}" RETURN p`;
    arangoFetch(arangoHelper.databaseUrl(url), {
      method: "GET"
    })
    .then(response => response.json())
    .then(data => {
      const mergedNodes = graphData.nodes.concat(data.nodes);
      const mergedEdges = graphData.edges.concat(data.edges);
      const uniqueMergedNodes = uniqBy(mergedNodes, 'id');
      const uniqueMergedEdges = uniqBy(mergedEdges, 'id');
      const newGraphData = {
        nodes: [
          ...uniqueMergedNodes
        ],
        edges: [
          ...uniqueMergedEdges
        ]
      };
      
      if(urlParameters.nodeSize) {
        setNodesSizeMinMax(data.settings.nodesSizeMinMax);
      }
      if(urlParameters.nodeSizeByEdges) {
        if(typeof connectionsMinMax == "undefined") {
          setConnectionsMinMax(data.settings.connectionsMinMax);
        } else {
          if(data.settings.connectionsMinMax[0] < connectionsMinMax[0]) {
            setConnectionsMinMax([data.settings.connectionsMinMax[0], connectionsMinMax[1]]);
          }
          if(data.settings.connectionsMinMax[1] > connectionsMinMax[1]) {
            setConnectionsMinMax([connectionsMinMax[0], data.settings.connectionsMinMax[1]]);
          }
        }
      }
      setGraphData(newGraphData);
    })
    .catch((err) => {
      arangoHelper.arangoError('Graph', 'Could not expand node.');
      console.log(err);
    });
  }

  const setStartnode = (node) => {
    const url = `/_admin/aardvark/g6graph/${graphName}?nodeLabelByCollection=true&nodeColorByCollection=false&nodeSizeByEdges=true&edgeLabelByCollection=true&edgeColorByCollection=false&nodeStart=${node}&depth=1&limit=2&nodeLabel=&nodeColor=#2ecc71&nodeColorAttribute=&nodeSize=&edgeLabel=&edgeColor=#cccccc&edgeColorAttribute=&edgeEditable=true`;
    arangoFetch(arangoHelper.databaseUrl(url), {
      method: "GET"
    })
    .then(response => response.json())
    .then(data => {
      setGraphData(data);
    })
    .catch((err) => {
      arangoHelper.arangoError('Graph', 'Could not load graph starting with this startnode.');
      console.log(err);
    });
  }

  const receiveDrawnGraph = (drawnGraph) => {
    drawnGraph.downloadImage();
  }

  const onDeleteEdge = (removedEdge) => {
    setShowDeleteEdgeModal(false);
    const edgesWithoutRemovedEdge = graphData.edges.filter(edges => edges.id !== removedEdge);
    const currentNodes = graphData.nodes;
    const currentSettings = graphData.settings;

    const newGraphData = {
      nodes: [
        ...currentNodes
      ],
      edges: [
        ...edgesWithoutRemovedEdge
      ],
      settings: [
        currentSettings
      ]
    };

    setGraphData(newGraphData);
  }
   
  */

  const lookUpDocument = (document) => {
    const documentId = document.item._cfg.id;
    const slashPos = documentId.indexOf("/");
    const documentDataObject = {
      "keys": [
        documentId.substring(slashPos + 1)
      ],
      "collection": documentId.substring(0, slashPos)
    };

    arangoFetch(arangoHelper.databaseUrl("/_api/simple/lookup-by-keys"), {
      method: "PUT",
      body: JSON.stringify(documentDataObject),
    })
    .then(response => response.json())
    .then(data => {
      console.log("looked up data: ", data);
      const attributes = data.documents[0];
      const allowedAttributesList = omit(attributes, '_rev', '_key');
      setLookedUpData(allowedAttributesList);
    })
    .catch((err) => {
      arangoHelper.arangoError('Graph', 'Could not look up this document.');
      console.log(err);
    });
  }

  return (
    <div>
      <QueryGraphView
        data={graphData}
        onClickDocument={(document) => lookUpDocument(document)}
      />
    </div>
  );
}

export default G6JsQueryGraph;
