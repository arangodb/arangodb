/* global arangoHelper, arangoFetch, $ */

import React, { useCallback, useEffect, useState } from 'react';
import { GraphView } from './GraphView';
import { data } from './data';
import { UrlParametersContext } from "./url-parameters-context";
import { AddNodeModal } from './AddNodeModal';
import { AddEdgeModal } from './AddEdgeModal';
import { EditNodeModal } from './EditNodeModal';
import { EditEdgeModal } from './EditEdgeModal';
import { DeleteNodeModal } from './DeleteNodeModal';
import { DeleteEdgeModal } from './DeleteEdgeModal';
import { DocumentInfo } from './DocumentInfo';
import URLPARAMETERS from "./UrlParameters";
import { omit, pick, uniqBy } from "lodash";

const VisJsGraph = () => {
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

  const [lookedUpData, setLookedUpData] = useState([]);
  const [nodesColorAttributes, setNodesColorAttributes] = useState({});
  const [edgesColorAttributes, setEdgesColorAttributes] = useState({});
  const [responseTimes, setResponseTimes] = useState(responseTimesObject);
  const [visQueryString, setVisQueryString] = useState(`/_admin/aardvark/visgraph/${graphName}`);
  const [visQueryMethod, setVisQueryMethod] = useState("GET");
  const [graphData, setGraphData] = useState(data);
  let [visGraphData, setVisGraphData] = useState(data);
  const [showFetchFullGraphModal, setShowFetchFullGraphModal] = useState(false);
  const [showEditNodeModal, setShowEditNodeModal] = useState(false);
  const [showEditEdgeModal, setShowEditEdgeModal] = useState(false);
  const [showDeleteEdgeModal, setShowDeleteEdgeModal] = useState(false);
  const [showDeleteNodeModal, setShowDeleteNodeModal] = useState(false);
  const [vertexCollections, setVertexCollections] = useState([]);
  const [vertexCollectionsColors, setVertexCollectionsColors] = useState();
  const [nodesSizeMinMax, setNodesSizeMinMax] = useState();
  const [connectionsMinMax, setConnectionsMinMax] = useState();
  const [edgeCollections, setEdgeCollections] = useState([]);
  const [edgeModelToAdd, setEdgeModelToAdd] = useState({});
  const [nodeToEdit, setNodeToEdit] = useState({});
  const [edgeToEdit, setEdgeToEdit] = useState({});
  const [edgeToDelete, setEdgeToDelete] = useState({});
  const [nodeToDelete, setNodeToDelete] = useState({});
  const [nodeDataToEdit, setNodeDataToEdit] = useState();
  const [basicNodeDataToEdit, setBasicNodeDataToEdit] = useState([]);
  const [edgeDataToEdit, setEdgeDataToEdit] = useState();
  const [edgeDataToDelete, setEdgeDataToDelete] = useState();
  const [nodeDataToDelete, setNodeDataToDelete] = useState();
  const [basicEdgeDataToEdit, setBasicEdgeDataToEdit] = useState([]);
  const [basicEdgeDataToDelete, setBasicEdgeDataToDelete] = useState([]);
  const [basicNodeDataToDelete, setBasicNodeDataToDelete] = useState([]);
  const [nodeKey, setNodeKey] = useState('');
  const [edgeKey, setEdgeKey] = useState('');
  const [edgeToDeleteKey, setEdgeToDeleteKey] = useState('');
  const [nodeToDeleteKey, setNodeToDeleteKey] = useState('');
  const [nodeCollection, setNodeCollection] = useState('');
  const [edgeCollection, setEdgeCollection] = useState('');
  const [edgeToDeleteCollection, setEdgeToDeleteCollection] = useState('');
  const [nodeToDeleteCollection, setNodeToDeleteCollection] = useState('');
  const [nodeToAdd, setNodeToAdd] = useState();
  const [isNewNodeToEdit, setIsNewNodeToEdit] = useState(false);
  const [showNodeToAddModal, setShowNodeToAddModal] = useState();
  const [showEdgeToAddModal, setShowEdgeToAddModal] = useState();

  const fetchVisData = useCallback(() => {
    responseTimesObject.fetchStarted = new Date();
    $.ajax({
      type: visQueryMethod,
      url: arangoHelper.databaseUrl(visQueryString),
      contentType: 'application/json',
      data: urlParameters,
      success: function (data) {
        responseTimesObject.fetchFinished = new Date();
        responseTimesObject.fetchDuration = Math.abs(responseTimesObject.fetchFinished.getTime() - responseTimesObject.fetchStarted.getTime());
        setResponseTimes(responseTimesObject);
        setVertexCollections(data.settings.vertexCollections);
        setEdgeCollections(data.settings.edgesCollections);
        setVisGraphData(data);
      },
      error: function (e) {
        arangoHelper.arangoError('Graph', e.responseJSON.errorMessage);
        console.log(e);
      }
    });
  }, [visQueryString]);

  useEffect(() => {
    fetchVisData();
  }, [fetchVisData]);

  const openAddNodeModal = () => {
    setNodeToAdd({});
    setShowNodeToAddModal(true);
  }

  const updateVisGraphDataWithNode = (newNode) => {
    const currentEdges = visGraphData.edges;
    const newGraphData = {
      nodes: [
        ...visGraphData.nodes,
        newNode
      ],
      edges: [
        ...currentEdges
      ]
    };
    setVisGraphData(newGraphData);
  }

  const updateVisGraphDataWithEdge = (newEdge) => {
    const currentNodes = visGraphData.nodes;
    const newGraphData = {
      nodes: [
        ...currentNodes,
      ],
      edges: [
        ...visGraphData.edges,
        newEdge
      ]
    };
    setVisGraphData(newGraphData);
  }

  const openAddEdgeModal = (newEdge) => {
    setShowEdgeToAddModal(true);
  }


  const openDeleteNodeModal = (nodeId) => {
    const slashPos = nodeId.indexOf("/");
    setNodeToDelete(nodeId);
    setNodeToDeleteKey(nodeId.substring(slashPos + 1));
    setNodeToDeleteCollection(nodeId.substring(0, slashPos));
    const nodeDataObject = {
      "keys": [
        nodeId.substring(slashPos + 1)
      ],
      "collection": nodeId.substring(0, slashPos)
    };

    arangoFetch(arangoHelper.databaseUrl("/_api/simple/lookup-by-keys"), {
      method: "PUT",
      body: JSON.stringify(nodeDataObject),
    })
    .then(response => response.json())
    .then(data => {
      const allowedDocuments = omit(data.documents[0], '_id', '_key', '_rev');
      setNodeDataToDelete(allowedDocuments);
      setBasicNodeDataToDelete(pick(data.documents[0], ['_id', '_key', '_rev']));
    })
    .catch((err) => {
      arangoHelper.arangoError('Graph', 'Could not look up this node.');
      console.log(err);
    });
    setShowDeleteNodeModal(true);
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

  const updateGraphDataWithNode = (newNode) => {
    const currentEdges = visGraphData.edges;
    const newGraphData = {
      nodes: [
        ...visGraphData.nodes,
        newNode
      ],
      edges: [
        ...currentEdges
      ]
    };
    setVisGraphData(newGraphData);
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

  const openEditNodeModal = (nodeId) => {
    const slashPos = nodeId.indexOf("/");
    setNodeToEdit(nodeId);
    setNodeKey(nodeId.substring(slashPos + 1));
    setNodeCollection(nodeId.substring(0, slashPos));
    const nodeDataObject = {
      "keys": [
        nodeId.substring(slashPos + 1)
      ],
      "collection": nodeId.substring(0, slashPos)
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

  const openEditEdgeModal = (edgeId) => {
    const slashPos = edgeId.indexOf("/");
    setEdgeToEdit(edgeId);
    setEdgeKey(edgeId.substring(slashPos + 1));
    setEdgeCollection(edgeId.substring(0, slashPos));
    const edgeDataObject = {
      "keys": [
        edgeId.substring(slashPos + 1)
      ],
      "collection": edgeId.substring(0, slashPos)
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

  const expandNode = (nodeId) => {
    const paramsObj = {
      depth: `${urlParameters.depth}`,
      limit: `${urlParameters.limit}`,
      nodeColor: `#${urlParameters.nodeColor}`,
      nodeColorAttribute: `${urlParameters.nodeColorAttribute}`,
      nodeColorByCollection: `${urlParameters.nodeColorByCollection}`,
      edgeColor: `#${urlParameters.edgeColor}`,
      edgeColorAttribute: `${urlParameters.edgeColorAttribute}`,
      edgeColorByCollection: `${urlParameters.edgeColorByCollection}`,
      nodeLabel: `${urlParameters.nodeLabel}`,
      edgeLabel: `${urlParameters.edgeLabel}`,
      nodeSize: `${urlParameters.nodeSize}`,
      nodeSizeByEdges: `${urlParameters.nodeSizeByEdges}`,
      edgeEditable: `${urlParameters.edgeEditable}`,
      nodeLabelByCollection: `${urlParameters.nodeLabelByCollection}`,
      edgeLabelByCollection: `${urlParameters.edgeLabelByCollection}`,
      nodeStart: `${urlParameters.nodeStart}`,
      barnesHutOptimize: `${urlParameters.barnesHutOptimize}`,
      query: "FOR v, e, p IN 1..1 ANY '" + nodeId + "' GRAPH '" + graphName + "' RETURN p",
    };

    const searchParams = new URLSearchParams(paramsObj);
    const url = `/_admin/aardvark/visgraph/${graphName}?${searchParams.toString()}`;
    arangoFetch(arangoHelper.databaseUrl(url), {
      method: "GET"
    })
    .then(response => response.json())
    .then(data => {
      const mergedNodes = visGraphData.nodes.concat(data.nodes);
      const mergedEdges = visGraphData.edges.concat(data.edges);
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
      setVisGraphData(newGraphData);
    })
    .catch((err) => {
      arangoHelper.arangoError('Graph', 'Could not expand node.');
      console.log(err);
    });
  }

  const setStartnode = (nodeId) => {
    const newUrlParameters = urlParameters;
    newUrlParameters.nodeStart = nodeId;
    setUrlParameters(newUrlParameters);

    const paramsObj = {
      depth: `${urlParameters.depth}`,
      limit: `${urlParameters.limit}`,
      nodeColor: `#${urlParameters.nodeColor}`,
      nodeColorAttribute: `${urlParameters.nodeColorAttribute}`,
      nodeColorByCollection: `${urlParameters.nodeColorByCollection}`,
      edgeColor: `#${urlParameters.edgeColor}`,
      edgeColorAttribute: `${urlParameters.edgeColorAttribute}`,
      edgeColorByCollection: `${urlParameters.edgeColorByCollection}`,
      nodeLabel: `${urlParameters.nodeLabel}`,
      edgeLabel: `${urlParameters.edgeLabel}`,
      nodeSize: `${urlParameters.nodeSize}`,
      nodeSizeByEdges: `${urlParameters.nodeSizeByEdges}`,
      edgeEditable: `${urlParameters.edgeEditable}`,
      nodeLabelByCollection: `${urlParameters.nodeLabelByCollection}`,
      edgeLabelByCollection: `${urlParameters.edgeLabelByCollection}`,
      nodeStart: `${urlParameters.nodeStart}`,
      barnesHutOptimize: `${urlParameters.barnesHutOptimize}`
    };

    const searchParams = new URLSearchParams(paramsObj);
    const url = `/_admin/aardvark/visgraph/${graphName}?${searchParams.toString()}`;
    arangoFetch(arangoHelper.databaseUrl(url), {
      method: "GET"
    })
    .then(response => response.json())
    .then(data => {
      setVisGraphData(data);
    })
    .catch((err) => {
      arangoHelper.arangoError('Graph', 'Could not load graph starting with this startnode.');
      console.log(err);
    });
  }

  const receiveDrawnGraph = (drawnGraph) => {
    drawnGraph.downloadImage();
  }

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
      const attributes = data.documents[0];
      const allowedAttributesList = omit(attributes, '_rev', '_key');
      setLookedUpData(allowedAttributesList);
    })
    .catch((err) => {
      arangoHelper.arangoError('Graph', 'Could not look up this document.');
      console.log(err);
    });
  }

  const lookUpDocumentForVis = (document) => {
    const slashPos = document.indexOf("/");
    const documentDataObject = {
      "keys": [
        document.substring(slashPos + 1)
      ],
      "collection": document.substring(0, slashPos)
    };

    arangoFetch(arangoHelper.databaseUrl("/_api/simple/lookup-by-keys"), {
      method: "PUT",
      body: JSON.stringify(documentDataObject),
    })
    .then(response => response.json())
    .then(data => {
      const attributes = data.documents[0];
      const allowedAttributesList = omit(attributes, '_rev', '_key');
      setLookedUpData(allowedAttributesList);
    })
    .catch((err) => {
      arangoHelper.arangoError('Graph', 'Could not look up this document.');
      console.log(err);
    });
  }

  const onDeleteNode = (nodeId) => {
    setShowDeleteNodeModal(false);
    const nodesWithoutRemovedNode = visGraphData.nodes.filter(nodes => nodes.id !== nodeId);
    const currentEdges = visGraphData.edges;
    const currentSettings = visGraphData.settings;

    const newGraphData = {
      nodes: [
        ...nodesWithoutRemovedNode
      ],
      edges: [
        ...currentEdges
      ],
      settings: currentSettings
    };

    setVisGraphData(newGraphData);
  }

  const onDeleteEdge = (edgeId) => {
    setShowDeleteEdgeModal(false);
    const edgesWithoutRemovedEdge = visGraphData.edges.filter(edges => edges.id !== edgeId);
    const currentNodes = visGraphData.nodes;
    const currentSettings = visGraphData.settings;

    const newGraphData = {
      nodes: [
        ...currentNodes
      ],
      edges: [
        ...edgesWithoutRemovedEdge
      ],
      settings: currentSettings
    };

    setVisGraphData(newGraphData);
  }

  return (
    <div>
      <UrlParametersContext.Provider value={[urlParameters, setUrlParameters]}>
        <DeleteEdgeModal
          shouldShow={showDeleteEdgeModal}
          onRequestClose={() => {
            setShowDeleteEdgeModal(false);
            setEdgeDataToDelete(undefined);
          }}
          edge={edgeToDelete}
          edgeData={edgeDataToDelete}
          basicEdgeData={basicEdgeDataToDelete}
          editorContent={edgeToDelete}
          onDeleteEdge={(edgeId) => onDeleteEdge(edgeId)}
          edgeToDeleteKey={edgeToDeleteKey}
          edgeToDeleteCollection={edgeToDeleteCollection}
        >
          <strong>Delete edge: {edgeToDelete}</strong>
        </DeleteEdgeModal>
        <DeleteNodeModal
          shouldShow={showDeleteNodeModal}
          onRequestClose={() => {
            setShowDeleteNodeModal(false);
            setNodeDataToDelete(undefined);
          }}
          node={nodeToDelete}
          nodeData={nodeDataToDelete}
          basicNodeData={basicNodeDataToDelete}
          editorContent={nodeToDelete}
          onDeleteNode={(nodeId) => onDeleteNode(nodeId)}
          nodeToDeleteKey={nodeToDeleteKey}
          nodeToDeleteCollection={nodeToDeleteCollection}
          graphName={graphName}
        >
          <strong>Delete node: {nodeToDelete}</strong>
        </DeleteNodeModal>
        <AddNodeModal
          shouldShow={showNodeToAddModal}
          onRequestClose={() => {
            setShowNodeToAddModal(false);
          }}
          node={'nodeToEdit'}
          vertexCollections={vertexCollections}
          nodeData={{}}
          editorContent={'nodeToEdit'}
          onAddNode={() => {
            setShowNodeToAddModal(false);
          }}
          onNodeCreation={(newNode) => {
            updateVisGraphDataWithNode(newNode)
          }}
          graphName={graphName}
          graphData={visGraphData}
        >
          <strong>Add node</strong>
        </AddNodeModal>
        <AddEdgeModal
          edgeModelToAdd={edgeModelToAdd}
          shouldShow={showEdgeToAddModal}
          onRequestClose={(isTriggeredOnSave) => {
            setShowEdgeToAddModal(false);
            //!isTriggeredOnSave && removeDrawnEdge();
          }}
          edgeCollections={edgeCollections}
          editorContent={'edgeToEdit'}
          onAddEdge={() => {
            setShowEdgeToAddModal(false);
          }}
          onEdgeCreation={(newEdge) => {
            updateVisGraphDataWithEdge(newEdge)
          }}
          edgeData={{}}
          graphName={graphName}
          graphData={graphData}
        >
          <strong>Add edge</strong>
        </AddEdgeModal>
        <EditNodeModal
          shouldShow={showEditNodeModal}
          onRequestClose={() => {
            setShowEditNodeModal(false);
            setNodeDataToEdit(undefined);
          }}
          node={nodeToEdit}
          nodeData={nodeDataToEdit}
          basicNodeData={basicNodeDataToEdit}
          editorContent={nodeToEdit}
          onUpdateNode={() => {
            setShowEditNodeModal(false);
          }}
          nodeKey={nodeKey}
          nodeCollection={nodeCollection}
        >
          <strong>Edit node: {nodeToEdit}</strong>
        </EditNodeModal>
        <EditEdgeModal
          shouldShow={showEditEdgeModal}
          onRequestClose={() => {
            setShowEditEdgeModal(false);
            setEdgeDataToEdit(undefined);
          }}
          edge={edgeToEdit}
          edgeData={edgeDataToEdit}
          basicEdgeData={basicEdgeDataToEdit}
          editorContent={edgeToEdit}
          onUpdateEdge={() => {
            setShowEditEdgeModal(false);
          }}
          edgeKey={edgeKey}
          edgeCollection={edgeCollection}
        >
          <strong>Edit edge: {edgeToEdit}</strong>
        </EditEdgeModal>
        <GraphView
          data={graphData}
          visGraphData={visGraphData}
          vertexCollections={vertexCollections}
          onUpdateNodeGraphData={(newGraphData) => updateGraphDataNodes(newGraphData)}
          onUpdateEdgeGraphData={(newGraphData) => updateGraphDataEdges(newGraphData)}
          onAddSingleNode={(newNode) => updateGraphDataWithNode(newNode)}
          onAddSingleEdge={(newEdge) => {
            setEdgeModelToAdd(newEdge);
            openAddEdgeModal(newEdge);
          }}
          onAddEdgeToDb={(edgeData) => {
            setEdgeModelToAdd(edgeData);
            openAddEdgeModal(edgeData);
          }}
          onRemoveSingleNode={(node) => removeNode(node)}
          onRemoveSingleEdge={(edge) => removeEdge(edge)}
          onEditNode={(nodeId) => openEditNodeModal(nodeId)}
          onEditEdge={(edgeId) => openEditEdgeModal(edgeId)}
          onDeleteEdge={(edgeId) => openDeleteEdgeModal(edgeId)}
          onAddNodeToDb={() => openAddNodeModal()}
          onExpandNode={(nodeId) => expandNode(nodeId)}
          onSetStartnode={(nodeId) => setStartnode(nodeId)}
          onGraphSending={(drawnGraph) => receiveDrawnGraph(drawnGraph)}
          graphName={graphName}
          responseDuration={responseTimes.fetchDuration}
          onChangeGraphData={(newGraphData) => setGraphData(newGraphData)}
          onClickDocument={(document) => lookUpDocument(document)}
          onClickNode={(nodeId) => lookUpDocumentForVis(nodeId)}
          onClickEdge={(edgeId) => lookUpDocumentForVis(edgeId)}
          onDeleteNode={(nodeId) => openDeleteNodeModal(nodeId)}
          onLoadFullGraph={() => setShowFetchFullGraphModal(true)}
          onGraphDataLoaded={({newGraphData, responseTimesObject}) => {
            setVisGraphData(newGraphData);
            setResponseTimes(responseTimesObject);
          }}
          vertexCollectionsColors={vertexCollectionsColors}
          nodeColor={urlParameters.nodeColor}
          edgeType={urlParameters.edgeType}
          nodesColorAttributes={nodesColorAttributes}
          nodeColorAttribute={urlParameters.nodeColorAttribute}
          nodeColorByCollection={urlParameters.nodeColorByCollection}
          edgesColorAttributes={edgesColorAttributes}
          edgeColorAttribute={urlParameters.edgeColorAttribute}
          edgeColorByCollection={urlParameters.edgeColorByCollection}
          nodesSizeMinMax={nodesSizeMinMax}
          connectionsMinMax={connectionsMinMax}
        />
        <DocumentInfo attributes={lookedUpData} />
      </UrlParametersContext.Provider>
    </div>
  );
}

export default VisJsGraph;
