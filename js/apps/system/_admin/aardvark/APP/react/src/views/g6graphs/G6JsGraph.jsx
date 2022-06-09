/* global arangoHelper, arangoFetch, frontendConfig */

import React, { useCallback, useEffect, useState, useContext } from 'react';
import ReactDOM from 'react-dom';
import { GraphView } from './GraphView';
import { AttributesInfo } from './AttributesInfo';
import { data } from './data';
import { data2 } from './data2';
import G6 from '@antv/g6';
import { Card, Tag } from 'antd';
import NodeStyleSelector from './NodeStyleSelector.js';
import { NodeList } from './components/node-list/node-list.component';
import { EdgeList } from './components/edge-list/edge-list.component';
import EdgeStyleSelector from './EdgeStyleSelectorOld';
import NodeLabelContentSelector from './NodeLabelContentSelector.js';
import { EditNodeModal } from './EditNodeModal';
import { Headerinfo } from './Headerinfo';
import { LoadDocument } from './LoadDocument';
import { EditModal } from './EditModal';
import { EditEdgeModal } from './EditEdgeModal';
import { AddNodeModal2 } from './AddNodeModal2';
import { AddEdgeModal } from './AddEdgeModal';
import { FetchFullGraphModal } from './FetchFullGraphModal';
import AqlEditor from './AqlEditor';
import { SlideInMenu } from './SlideInMenu';
import { omit, pick, uniqBy } from "lodash";
import { JsonEditor as Editor } from 'jsoneditor-react';
import { UrlParametersContext } from "./url-parameters-context";
import URLPARAMETERS from "./UrlParameters";
import ButtonScrollTo from "./ButtonScrollTo";
import FetchData from "./FetchData";
import G6FuncComponent from "./G6FuncComponent";
import G6GraphView from "./G6GraphView";
import './tooltip.css';

const G6JsGraph = () => {
  const [urlParameters, setUrlParameters] = React.useState(URLPARAMETERS);
  //const [contextUrlParameters, setContextUrlParameters] = useContext(UrlParametersContext);

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

  let apiParameters = Object.keys(urlParams)
  .map((key) => key + "=" + urlParams[key])
  .join("&");

  console.log("apiParameters 2: ", apiParameters);

  const testApiParams = () => {
    let apiParameters = Object.keys(urlParams)
    .map((key) => key + "=" + urlParams[key])
    .join("&");

    console.log("urlParams: ", urlParams);
    console.log("urlParams.nodeColor: ", urlParams.nodeColor);
    const testUrl = `/_admin/aardvark/g6graph/${graphName}?${apiParameters}`;
    console.log("testUrl: ", testUrl);
    arangoFetch(arangoHelper.databaseUrl(testUrl), {
      method: queryMethod,
    })
    .then(response => response.json())
    .then(data => {
      console.log("testApiParams: ", data);
      setGraphData(data);
    })
    .catch((err) => {
      console.log(err);
    });
  }

  const currentUrlParams = () => {
    console.log("currentUrlParams: ", urlParams);
  }

  const [responseTimes, setResponseTimes] = useState(responseTimesObject);
  const currentUrl = window.location.href;
  const [graphName, setGraphName] = useState(currentUrl.substring(currentUrl.lastIndexOf("/") + 1));
  console.log('graphName: ', graphName);
  let [queryString, setQueryString] = useState(`/_admin/aardvark/g6graph/${graphName}`);
  let [queryMethod, setQueryMethod] = useState("GET");
  let [graphData, setGraphData] = useState(data);
  const ref = React.useRef(null);
  let graph = null;
  const [showFetchFullGraphModal, setShowFetchFullGraphModal] = useState(false);
  const [showEditNodeModal, setShowEditNodeModal] = useState(false);
  const [showEditModal, setShowEditModal] = useState(false);
  const [showEditEdgeModal, setShowEditEdgeModal] = useState(false);
  const [editNode, setEditNode] = useState();
  const [vertexCollections, setVertexCollections] = useState([]);
  const [vertexCollectionsColors, setVertexCollectionsColors] = useState();
  const [nodesSizeMinMax, setNodesSizeMinMax] = useState();
  const [connectionsMinMax, setConnectionsMinMax] = useState();
  const [edgeCollections, setEdgeCollections] = useState([]);
  const [edgeModelToAdd, setEdgeModelToAdd] = useState([]);
  const [nodeToEdit, setNodeToEdit] = useState({});
  /*
  const [nodeToEdit, setNodeToEdit] = useState(
  {
    "keys": [
      "Hamburg"
    ],
    "collection": "germanCity"
  });
  */
  const [edgeToEdit, setEdgeToEdit] = useState(
    {
      "keys": [
        "Hamburg"
      ],
      "collection": "germanCity"
    });
  const [nodeDataToEdit, setNodeDataToEdit] = useState();
  const [basicNodeDataToEdit, setBasicNodeDataToEdit] = useState([]);
  const [edgeDataToEdit, setEdgeDataToEdit] = useState();
  const [basicEdgeDataToEdit, setBasicEdgeDataToEdit] = useState([]);
  const [nodeKey, setNodeKey] = useState('Hamburg');
  const [edgeKey, setEdgeKey] = useState('Hamburg');
  const [nodeCollection, setNodeCollection] = useState('germanCity');
  const [edgeCollection, setEdgeCollection] = useState('germanHighway');
  const [selectedNodeData, setSelectedNodeData] = useState();
  const [nodeToAdd, setNodeToAdd] = useState();
  const [nodeToAddKey, setNodeToAddKey] = useState("Munich");
  const [nodeToAddCollection, setNodeToAddCollection] = useState("germanCity");
  const [nodeToAddData, setNodeToAddData] = useState();
  const [isNewNodeToEdit, setIsNewNodeToEdit] = useState(false);
  const [showNodeToAddModal, setShowNodeToAddModal] = useState();
  const [showEdgeToAddModal, setShowEdgeToAddModal] = useState();
  const [type, setLayout] = React.useState('graphin-force');
  let [aqlQueryString, setAqlQueryString] = useState("/_admin/aardvark/g6graph/routeplanner");

  const randomColor = () => {
    return Math.floor(Math.random()*16777215).toString(16);
  }

  const handleChange = value => {
    console.log('handleChange (value): ', value);
    //setLayout(value);
  };

  // fetching data # start
  const fetchData = useCallback(() => {
    responseTimesObject.fetchStarted = new Date();
    arangoFetch(arangoHelper.databaseUrl(queryString), {
      method: queryMethod,
    })
    .then(response => response.json())
    .then(data => {
      responseTimesObject.fetchFinished = new Date();
      responseTimesObject.fetchDuration = Math.abs(responseTimesObject.fetchFinished.getTime() - responseTimesObject.fetchStarted.getTime());
      setResponseTimes(responseTimesObject);
      console.log("NEW DATA for graphData: ", data);
      console.log("data.settings: ", data.settings);
      console.log("data.settings.vertexCollections: ", data.settings.vertexCollections);
      setVertexCollections(data.settings.vertexCollections);

      const collectionColors = [];
      Object.keys(data.settings.vertexCollections)
      .map((key, i) => {
        const collectionName = data.settings.vertexCollections[key].name;
        collectionColors[data.settings.vertexCollections[key].name] = "#" + randomColor();
        return true;
      });
      console.log("collectionColors: ", collectionColors);
      setVertexCollectionsColors(collectionColors);

      setEdgeCollections(data.settings.edgeCollections);
      console.log("data.settings.edgeCollections: ", data.settings.edgeCollections);
      setGraphData(data);
    })
    .catch((err) => {
      console.log(err);
    });
  }, [queryString]);

  useEffect(() => {
    fetchData();
  }, [fetchData]);
  // fetching data # end

  // fetching edge collections # start
  const fetchEdgeCollections = useCallback(() => {
    console.log("#######FETCHING COLLECTIONS##########");
    arangoFetch(arangoHelper.databaseUrl('/_api/collection'), {
      method: "GET"
    })
    .then(response => response.json())
    .then(data => {
      console.log("returned COLLECTIONS: ", data);
      console.log("data.result: ", data.result);
      const returnedEdgeCollections = data.result.filter(collection => collection.type === 3);
      console.log("returnedEdgeCollections: ", returnedEdgeCollections);
      setEdgeCollections(returnedEdgeCollections);
    })
    .catch((err) => {
      console.log(err);
    });
  }, []);

  useEffect(() => {
    fetchEdgeCollections();
  }, [fetchEdgeCollections]);
  // fetching edge collections # end

  // Instantiate the Minimap
  const minimap = new G6.Minimap({
    size: [100, 100],
    className: 'minimap',
    type: 'delegate',
  });

  // Instantiate the Toolbar
  /*
  const toolbar = new G6.ToolBar({
    position: { x: 10, y: 70 },
  });
  */

  // Instantiate grid
  const grid = new G6.Grid();

  /*
  useEffect(() => {
    if (!graph) {
        graph = new G6.Graph({
          plugins: [minimap], // Configure toolbar and minimap to the graph
          enabledStack: true,
          container: ReactDOM.findDOMNode(ref.current),
          width: 1200,
          height: 400,
          layout: {
            type: 'gForce',
            minMovement: 0.01,
            maxIteration: 100,
            preventOverlap: true,
            damping: 0.99,
            fitView: true,
            linkDistance: 100
          },
          modes: {
            default: [
              'drag-canvas',
              'zoom-canvas',
              'drag-node',
              {
                type: 'tooltip', // Tooltip
                formatText(model) {
                  // The content of tooltip
                  const text = 'label: ' + model.label + ((model.population !== undefined) ? ('<br />population: ' + model.population) : ('<br/> population: No information '));
                  return text;
                },
              },
              {
                type: 'edge-tooltip', // Edge tooltip
                formatText(model) {
                  // The content of the edge tooltip
                  const text =
                    'source: ' +
                    model.source +
                    '<br/> target: ' +
                    model.target +
                    '<br/> type: ' +
                    model.data.type +
                    '<br/> distance: ' +
                    model.data.distance +
                    '<br/> date: ' +
                    model.data.date;
                  return text;
                },
              },
            ], // Allow users to drag canvas, zoom canvas, and drag nodes
          },
          defaultNode: {
            type: 'circle', // 'bubble'
            size: 30,
            labelCfg: {
              position: 'center',
              style: {
                fill: 'blue',
                fontStyle: 'bold',
                fontFamily: 'sans-serif',
                fontSize: 12
              },
            },
          },
          nodeStateStyles: {
          // node style of active state
            active: {
              fillOpacity: 0.8,
            },
          // node style of selected state
            selected: {
              lineWidth: 5,
            },
          },
        });
    }
    graph.data(data);
    //graph.data(graphData);
    graph.render();
  }, []);
  */

  const changeCollection = (myString) => {
    console.log('in changeCollection');
    setAqlQueryString(`/_admin/aardvark/g6graph/${myString}`);
  }

  const getNodes = () => {
    const nodes = graph.getNodes();
    console.log("getNodes(): ", nodes);
  }

  const getEdges = () => {
    const edges = graph.getEdges();
    console.log("getEdges(): ", edges);
  }

  const changeGraphData = () => {
    //graph.changeData(data2);
    setGraphData(data2);
  }

  const addNode = () => {
    const nodeModel = {
      id: 'newnode',
      label: 'newnode',
      address: 'cq',
      x: 200,
      y: 150,
      style: {
        fill: 'white',
      },
    };
    
    graph.addItem('node', nodeModel);
  }

  const addEdge = () => {
    const edgeModel = {
      source: 'newnode',
      target: '2',
      data: {
        type: 'name1',
        amount: '1000,00 元',
        date: '2022-01-13'
      }
    };
    
    graph.addItem('edge', edgeModel);
  }

  const updateEdgeModel = () => {
    const model = {
      id: '2',
      label: 'node2',
      distance: '2,950,000',
      type: 'diamond',
      style: {
        fill: 'red',
      },
    };
    
    // Find the item instance by id
    const item = graph.findById('2');
    graph.updateItem(item, model);
  }

  const changeNodeStyle = (typeModel) => {
    graph.node((node) => {
      return {
        id: node.id,
        ...typeModel
      };
    });
    
    graph.data(data);
    graph.render();
  }

  const changeEdgeStyle = (typeModel) => {
    graph.edge((edge) => {
      return {
        id: edge.id,
        ...typeModel
      };
    });

    graph.data(data);
    graph.render();
  }

  const changeNodeLabelContent = (typeModel) => {
    console.log('changeNodeLabelContent (typeModel: ', typeModel);
    graph.node((node) => {
      return {
        id: node.id,
        ...typeModel
      };
    });

    graph.data(data);
    graph.render();
  }

  const changeCollectionName = (typeModel) => {
    console.log('changeCollectionName (typeModel: ', typeModel);
    graph.node((node) => {
      return {
        id: node.id,
        ...typeModel
      };
    });

    graph.data(data);
    graph.render();
  }

  const returnGraph = () => {
    console.log('graph is: ', graph);
  }

  const updateGraphDataWithNode = (newNode) => {
    console.log("...........updateGraphDataWithNode (newNode): ", newNode);
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
    console.log("...........newGraphData (newNode): ", newGraphData);
    setGraphData(newGraphData);
  }

  const updateGraphDataWithEdge = (newEdge) => {
    console.log("...........updateGraphDataWithEdge (newEdge): ", newEdge);
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

  const printGraphData = () => {
    console.log("vertexCollections: ", vertexCollections);
    console.log("vertexCollectionsColors.frenchCity: ", vertexCollectionsColors.frenchCity);
    console.log("vertexCollectionsColors.germanCity: ", vertexCollectionsColors.germanCity);
    console.log("Current graphData: ", graphData);
  }

  const removeNode = (nodeId) => {
    const nodes = graphData.nodes;
    console.log("removeNode (nodes):", nodes);
    console.log("removeNode: ", nodeId);
    const updatedNodes = nodes.filter(item => item.id !== nodeId);
    console.log("updatedNodes: ", updatedNodes);
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
    console.log("removeEdge (edges):", edges);
    console.log("removeEdge: ", edgeId);
    const updatedEdges = edges.filter(item => item.id !== edgeId);
    console.log("updatedEdges: ", updatedEdges);
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

  const openEditNodeModal = () => {
    setShowEditNodeModal(true);
  }

  const openEditModal = (node) => {
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
      console.log("Error looking up this node: ", error);
      setShowEditModal(false);
    });

    setIsNewNodeToEdit(true);
    setShowEditModal(true);
  }

  const openEditEdgeModal = (edge) => {
    console.log(">>>>>>>>>>>> openEditEdgeModal (edge): ", edge);
    const slashPos = edge.indexOf("/");
    setEdgeToEdit(edge);
    console.log(">>>>>>>>>>>> openEditEdgeModal (edgeKey): ", edge.substring(slashPos + 1));
    setEdgeKey(edge.substring(slashPos + 1));
    console.log(">>>>>>>>>>>> openEditEdgeModal (edgeCollection): ", edge.substring(0, slashPos));
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
        console.log("Document info loaded before opening EditEdgeModal component: ");
        console.log("Document info loaded before opening EditEdgeModal component (data): ", data);
        console.log("Document info loaded before opening EditEdgeModal component (data.documents): ", data.documents);
        console.log("Document info loaded before opening EditEdgeModal component (data.documents[0]): ", data.documents[0]);
        const allowedDocuments = omit(data.documents[0], '_id', '_key', '_rev', '_from', '_to');
        console.log("allowedDocuments: ", allowedDocuments);
        setBasicEdgeDataToEdit(pick(data.documents[0], ['_id', '_key', '_rev', '_from', '_to']));
        //setEdgeDataToEdit(data.documents[0]);
        setEdgeDataToEdit(allowedDocuments);
      })
      .catch((err) => {
        console.log(err);
      });
    console.log("openEditEdgeModal: change edgeToEdit now");

    setShowEditEdgeModal(true);
  }

  const openAddEdgeModal = (newEdge) => {
    console.log(">>>>>>>>>>>> openAddEdgeModal");
    const data = [{
      "keys": [
        "Munich"
      ],
      "collection": "germanCity"
    }];
    //setNodeToAdd(data);
    //setNodeToAddKey("Munich");
    //setNodeToAddCollection("germanCity");
    const nodeToAddDataObject = {
      "keys": [
        "Munich"
      ],
      "collection": "germanCity"
    };
    /*
      arangoFetch(arangoHelper.databaseUrl("/_api/simple/lookup-by-keys"), {
        method: "POST",
        body: JSON.stringify(nodeToAddDataObject),
      })
      .then(response => response.json())
      .then(data => {
        const allowedDocuments = omit(data.documents[0], '_id', '_key');
        console.log("allowedDocuments: ", allowedDocuments);
        setNodeToAddData(data.documents[0]);
        //setNodeDataToEdit(data);
      })
      .catch((err) => {
        console.log(err);
      });
    */
    console.log("openAddEdgeModal");
    //return <LoadDocument node={node} />
    console.log("setShowEdgeToAddModal (1): ", setShowEdgeToAddModal);
    setShowEdgeToAddModal(true);
    console.log("setShowEdgeToAddModal (2): ", setShowEdgeToAddModal);
  }

  const openAddNodeModal = () => {
    setNodeToAdd({});
    setShowNodeToAddModal(true);
  }

  const expandNode = (node) => {
    console.log("nodesColorAttributes: ", nodesColorAttributes);
    console.log("urlParameters from context at expanding node: ", urlParameters);
    console.log("urlParameters.edgeLabelByCollection: ", urlParameters.edgeLabelByCollection);
    console.log("urlParameters.nodeLabelByCollection: ", urlParameters.nodeLabelByCollection);
    console.log("urlParameters.nodeSizeByEdges: ", urlParameters.nodeSizeByEdges);
    // /_admin/aardvark/graph/routeplanner?nodeLabelByCollection=false&nodeColorByCollection=true&nodeSizeByEdges=true&edgeLabelByCollection=false&edgeColorByCollection=false&nodeStart=frenchCity/Caen&depth=2&limit=1&nodeLabel=_key&nodeColor=#2ecc71&nodeColorAttribute=&nodeSize=&edgeLabel=&edgeColor=#cccccc&edgeColorAttribute=&edgeEditable=true&query=FOR v, e, p IN 1..1 ANY "germanCity/Hamburg" GRAPH "routeplanner" RETURN p
    console.log(">>>>>>>>>>>> expandNode (node): ", node);
    //const url = `/_admin/aardvark/graph/routeplanner?nodeLabelByCollection=false&nodeColorByCollection=true&nodeSizeByEdges=true&edgeLabelByCollection=false&edgeColorByCollection=false&nodeStart=frenchCity/Caen&depth=2&limit=250&nodeLabel=_key&nodeColor=#2ecc71&nodeColorAttribute=&nodeSize=&edgeLabel=&edgeColor=#cccccc&edgeColorAttribute=&edgeEditable=true&query=FOR v, e, p IN 1..1 ANY "frenchCity/Caen" GRAPH "routeplanner" RETURN p`;
    //const url = `/_admin/aardvark/graph/${graphName}?depth=2&limit=250&nodeColor=#2ecc71&nodeColorAttribute=&nodeColorByCollection=true&edgeColor=#cccccc&edgeColorAttribute=&edgeColorByCollection=false&nodeLabel=_key&edgeLabel=&nodeSize=&nodeSizeByEdges=true&edgeEditable=true&nodeLabelByCollection=false&edgeLabelByCollection=false&nodeStart=&barnesHutOptimize=true&query=FOR v, e, p IN 1..1 ANY "${node}" GRAPH "${graphName}" RETURN p`;
    //const url = `/_admin/aardvark/graph/${graphName}?depth=2&limit=250&nodeColor=%232ecc71&nodeColorAttribute=&nodeColorByCollection=false&edgeColor=%23cccccc&edgeColorAttribute=&edgeColorByCollection=false&nodeLabel=_key&edgeLabel=&nodeSize=&nodeSizeByEdges=${urlParameters.nodeSizeByEdges}&edgeEditable=true&nodeLabelByCollection=false&edgeLabelByCollection=false&nodeStart=&barnesHutOptimize=true&query=FOR v, e, p IN 1..1 ANY "${node}" GRAPH "${graphName}" RETURN p`;
    //const url = `/_admin/aardvark/g6graph/${graphName}?depth=${urlParameters.depth}&limit=${urlParameters.limit}&nodeColor=%23${urlParameters.nodeColor}&nodeColorAttribute=${urlParameters.nodeColorAttribute}&nodeColorByCollection=${urlParameters.nodeColorByCollection}&edgeColor=%23${urlParameters.edgeColor}&edgeColorAttribute=${urlParameters.edgeColorAttribute}&edgeColorByCollection=${urlParameters.edgeColorByCollection}&nodeLabel=${urlParameters.nodeLabel}&edgeLabel=${urlParameters.edgeLabel}&nodeSize=${urlParameters.nodeSize}&nodeSizeByEdges=${urlParameters.nodeSizeByEdges}&edgeEditable=${urlParameters.edgeEditable}&nodeLabelByCollection=${urlParameters.nodeLabelByCollection}&edgeLabelByCollection=${urlParameters.edgeLabelByCollection}&nodeStart=${urlParameters.nodeStart}&barnesHutOptimize=${urlParameters.barnesHutOptimize}&nodesColorAttributes=${JSON.stringify(nodesColorAttributes)}&query=FOR v, e, p IN 1..1 ANY "${node}" GRAPH "${graphName}" RETURN p`;
    const url = `/_admin/aardvark/g6graph/${graphName}?depth=${urlParameters.depth}&limit=${urlParameters.limit}&nodeColor=%23${urlParameters.nodeColor}&nodeColorAttribute=${urlParameters.nodeColorAttribute}&nodeColorByCollection=${urlParameters.nodeColorByCollection}&edgeColor=%23${urlParameters.edgeColor}&edgeColorAttribute=${urlParameters.edgeColorAttribute}&edgeColorByCollection=${urlParameters.edgeColorByCollection}&nodeLabel=${urlParameters.nodeLabel}&edgeLabel=${urlParameters.edgeLabel}&nodeSize=${urlParameters.nodeSize}&nodeSizeByEdges=${urlParameters.nodeSizeByEdges}&edgeEditable=${urlParameters.edgeEditable}&nodeLabelByCollection=${urlParameters.nodeLabelByCollection}&edgeLabelByCollection=${urlParameters.edgeLabelByCollection}&nodeStart=${urlParameters.nodeStart}&barnesHutOptimize=${urlParameters.barnesHutOptimize}&query=FOR v, e, p IN 1..1 ANY "${node}" GRAPH "${graphName}" RETURN p`;
      arangoFetch(arangoHelper.databaseUrl(url), {
        method: "GET"
      })
      .then(response => response.json())
      .then(data => {
        console.log("############ Received expanded garph data: ", data);
        console.log("Current nodes: ", graphData.nodes);
        console.log("Received nodes: ", data.nodes);
        console.log("Current edges: ", graphData.edges);
        console.log("Whatever there is");
        console.log("Received edges: ", data.edges);
        console.log("Received settings: ", data.settings);
        
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

          console.error("connectionsMinMax currently is: ", connectionsMinMax);

          if(typeof connectionsMinMax == "undefined") {
            console.error("connectionsMinMax is undefined");
            console.log("The new connectionsMinMax are: ", data.settings.connectionsMinMax);
            setConnectionsMinMax(data.settings.connectionsMinMax);
          } else {
            if(data.settings.connectionsMinMax[0] < connectionsMinMax[0]) {
              console.error("(HERE) The minimum of edgescount changed from " + connectionsMinMax[0] + " to " + data.settings.connectionsMinMax[0]);
              setConnectionsMinMax([data.settings.connectionsMinMax[0], connectionsMinMax[1]]);
            }
  
            if(data.settings.connectionsMinMax[1] > connectionsMinMax[1]) {
              console.error("(HERE) The maximum of edgescount changed from " + connectionsMinMax[1] + " to " + data.settings.connectionsMinMax[1]);
              setConnectionsMinMax([connectionsMinMax[0], data.settings.connectionsMinMax[1]]);
            }
          }

          /*
          ////////////
          if(typeof connectionsMinMax == "undefined") {
            console.error("connectionsMinMax is undefined");
            setConnectionsMinMax(newGraphData.settings.connectionsMinMax);
          }
          
          if(typeof connectionsMinMax != "undefined") {
            console.log("(HERE) newGraphData.settings.connectionsMinMax[0]: ", newGraphData.settings.connectionsMinMax[0]);
            console.log("(HERE) connectionsMinMax[0]: ", connectionsMinMax[0]);
            if(newGraphData.settings.connectionsMinMax[0] < connectionsMinMax[0]) {
              console.error("(HERE) The minimum of edgescount changed from " + connectionsMinMax[0] + " to " + newGraphData.settings.connectionsMinMax[0]);
              setConnectionsMinMax([newGraphData.settings.connectionsMinMax[0], connectionsMinMax[1]]);
            }
          }
          if(typeof connectionsMinMax != "undefined") {
            console.log("(HERE) newGraphData.settings.connectionsMinMax[1]: ", newGraphData.settings.connectionsMinMax[1]);
            console.log("(HERE) connectionsMinMax[1]: ", connectionsMinMax[1]);
            if(newGraphData.settings.connectionsMinMax[1] > connectionsMinMax[1]) {
              console.error("(HERE) The maximum of edgescount changed from " + connectionsMinMax[1] + " to " + newGraphData.settings.connectionsMinMax[1]);
              setConnectionsMinMax([connectionsMinMax[0], newGraphData.settings.connectionsMinMax[1]]);
            }
          }
          //////////////

          if(data.settings.connectionsMinMax[0]) {
            console.log("FOUND THE PLACE (data.settings.connectionsMinMax[0]): ", data.settings.connectionsMinMax[0]);
          }
          */
          //setConnectionsMinMax(data.settings.connectionsMinMax);
        }
        setGraphData(newGraphData);
      })
      .catch((err) => {
        console.log(err);
      });
    //setShowNodeToAddModal(true);
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
        console.log(err);
      });
  }

  const editNodeMode = (node) => {
    console.log('editNodeMode (node): ', node);
    //openeditmodal
  }


  const changeGraphNodes = () => {
    console.log("changeGraphNodes");
  }

  const getNewGraphData = (myString) => {
    console.log("getExpandedGraphData (myString): ", myString);
    //let url = `/_admin/aardvark/graph/${graphName}?depth=2&limit=250&nodeColor=#2ecc71&nodeColorAttribute=&nodeColorByCollection=true&edgeColor=#cccccc&edgeColorAttribute=&edgeColorByCollection=false&nodeLabel=_key&edgeLabel=&nodeSize=&nodeSizeByEdges=true&edgeEditable=true&nodeLabelByCollection=false&edgeLabelByCollection=false&nodeStart=&barnesHutOptimize=true&query=FOR v, e, p IN 1..1 ANY "${node}" GRAPH "${graphName}" RETURN p`;
    let url = myString;
    arangoFetch(arangoHelper.databaseUrl(url), {
      method: "GET"
    })
    .then(response => response.json())
    .then(data => {
      console.log("Received new graph data: ", data);
      setGraphData(data);
    })
    .catch((err) => {
      console.log(err);
    });
  }

  const receiveDrawnGraph = (drawnGraph) => {
    console.log("drawnnGraph in parent (G6JsGraph): ", drawnGraph);
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
        console.log("looked up allowedAttributesList: ", allowedAttributesList);
        setLookedUpData(allowedAttributesList);
      })
      .catch((err) => {
        console.log(err);
      });
  }

  /*
  <NodeStyleSelector onNodeStyleChange={(typeModel) => changeNodeStyle(typeModel)} />
        <EdgeStyleSelector onEdgeStyleChange={(typeModel) => changeEdgeStyle(typeModel)} />
        <AddCollectionNameSelector onAddCollectionNameChange={(typeModel) => changeCollectionName(typeModel)} />
        <NodeLabelContentSelector onNodeLabelContentChange={(typeModel) => changeNodeLabelContent(typeModel)} />
        <button onClick={() => getNodes()}>Get nodes</button>
        <button onClick={() => getEdges()}>Get edges</button>
        <button onClick={() => changeGraphData()}>Change graph data</button>
        <button onClick={() => addNode()}>Add node</button>
        <button onClick={() => addEdge()}>Add edge</button>
        <button onClick={() => returnGraph()}>What is graph?</button>
  */
 
        //<Editor value={selectedNodeData} onChange={() => console.log('Data in jsoneditor changed')} mode={'code'} history={true}/>
        //<LoadDocument node={nodeToEdit} />

        /*
        <button onClick={() => changeGraphData()}>Change graph data (parent)</button>
      <button onClick={() => openEditNodeModal()}>Open edit node modal</button>
      <button onClick={() => openEditModal()}>Open edit modal</button>
      <LayoutSelector options={layouts} value={type} onChange={handleChange} />
      <Headerinfo
        graphName={graphName}
        graphData={graphData}
        responseDuration={responseTimes.fetchDuration}
      />
      <G6FuncComponent
          data={graphData}
        />
        <FetchData />
        <SlideInMenu>
          <h5>Graph</h5>
          <AqlEditor
              queryString={queryString}
              onNewSearch={(myString) => {setQueryString(myString)}}
              onQueryChange={(myString) => {setQueryString(myString)}}
              onOnclickHandler={(myString) => changeCollection(myString)}
              onReduceNodes={(nodes) => changeGraphNodes(nodes)}
              onAqlQueryHandler={(myString) => getNewGraphData(myString)}
          />
          <h5>Nodes</h5>
          <NodeList
            nodes={graphData.nodes}
            graphData={graphData}
            onNodeInfo={() => console.log('onNodeInfo() in MenuGraph')}
          />
          <h5>Edges</h5>
          <EdgeList edges={graphData.edges} />
                      
        </SlideInMenu>
      */

        
        const changeGraphDataTest = () => {
          console.log("changeGraphDataTest");
          setGraphData(data2);
        }

        // <UrlParametersContext.Provider value={urlParameters}>
        //<AttributesInfo attributes={lookedUpData} />
        /*
        // Test Buttons
        <button onClick={() => printGraphData()}>Print graphData</button>
        <button onClick={() => currentUrlParams()}>current Url Params</button>
        <button onClick={() => testApiParams()}>Test API Params</button>
        <button onClick={() => changeGraphDataTest()}>Change graph data test</button>
        <button onClick={() => updateGraphDataWithEdge(edgeModelToAdd)}>Add edge to graph drawing</button>
        <div>urlParameters: {JSON.stringify(urlParameters)}</div>

        <h3>G6FuncComponent</h3>
        <G6FuncComponent
          data={graphData}
        />

        <h3>G6GraphView</h3>
        <G6GraphView data={graphData} />





        <button onClick={() => printGraphData()}>Print graphData</button>
        <button onClick={() => currentUrlParams()}>current Url Params</button>
        <button onClick={() => testApiParams()}>Test API Params</button>
        <button onClick={() => changeGraphDataTest()}>Change graph data test</button>
        <button onClick={() => updateGraphDataWithEdge(edgeModelToAdd)}>Add edge to graph drawing</button>
        <button onClick={() => setLookedUpData({'whetever': 'chocolate'})}>Change lookedupdata</button>
        <button onClick={() => console.log("nodesColorAttributes: ", nodesColorAttributes)}>Print nodesColorAttributes</button>
        <button onClick={() => console.log("nodesColorAttributes[0]: ", nodesColorAttributes[0])}>Print nodesColorAttributes[0]</button>
        <button onClick={() => console.log("nodesColorAttributes[0].color: ", nodesColorAttributes[0].color)}>Print nodesColorAttributes[0].color</button>
        <div>urlParameters: {JSON.stringify(urlParameters)}</div>

        <button onClick={() => {
          console.log("LocalStorage: ",localStorage.getItem("arangoviking"));
          console.log("nodesColorAttributes: ", nodesColorAttributes)
          }}>Print nodesColorAttributes</button>
        
        <button onClick={() => {
          console.log("connectionsMinMax: ", connectionsMinMax)
          }}>Print connectionsMinMax</button>
        
        */
        
  return (
    <div>
      <UrlParametersContext.Provider value={[urlParameters, setUrlParameters, vertexCollectionsColors]}>
      
        <EditModal
          shouldShow={showEditModal}
          onRequestClose={() => {
            setShowEditModal(false);
            setNodeDataToEdit(undefined);
          }}
          node={nodeToEdit}
          nodeData={nodeDataToEdit}
          basicNodeData={basicNodeDataToEdit}
          editorContent={nodeToEdit}
          onUpdateNode={() => {
            //setGraphData(newGraphData);
            setShowEditModal(false);
          }}
          nodeKey={nodeKey}
          nodeCollection={nodeCollection}
        >
          <strong>Edit node: {nodeToEdit}</strong>
        </EditModal>
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
            //setGraphData(newGraphData);
            setShowEditEdgeModal(false);
          }}
          edgeKey={edgeKey}
          edgeCollection={edgeCollection}
        >
          <strong>Edit edge: {edgeToEdit}</strong>
        </EditEdgeModal>
        <EditNodeModal
          graphData={graphData}
          shouldShow={showEditNodeModal}
          onRequestClose={() => {
            setShowEditNodeModal(false);
          }}
          node={editNode}
          onUpdateNode={(newGraphData) => {
            console.log('newGraphData in EditNode: ', newGraphData);
            //setGraphData(newGraphData);
            setShowEditNodeModal(false);
          }}
        >
          <strong>Edit Node (new)</strong>
          <strong>Really edit node: {editNode}</strong>
          <strong>Edit node (object): {JSON.stringify(selectedNodeData, null, 2)}</strong>
        </EditNodeModal>
        <FetchFullGraphModal
          shouldShow={showFetchFullGraphModal}
          onRequestClose={() => {
            setShowFetchFullGraphModal(false);
          }}
          onFullGraphLoaded={(newGraphData) => setGraphData(newGraphData)}
          graphName={graphName}
        >
          <strong>Fetch full graph</strong>
          <p><span class="text-error">Caution:</span> Really load full graph? If no limit is set, your result set could be too big.</p>
        </FetchFullGraphModal>
        <AddEdgeModal
          edgeModelToAdd={edgeModelToAdd}
          shouldShow={showEdgeToAddModal}
          onRequestClose={() => {
            setShowEdgeToAddModal(false);
          }}
          edgeCollections={edgeCollections}
          editorContent={'edgeToEdit'}
          onAddEdge={() => {
            setShowEdgeToAddModal(false);
          }}
          onEdgeCreation={(newEdge) => {
            console.log("newEdge in G6JsGraph: ", newEdge);
            updateGraphDataWithEdge(newEdge)
          }}
          edgeData={[]}
          graphName={graphName}
          graphData={graphData}
        >
          <strong>Add edge</strong>
        </AddEdgeModal>
        <AddNodeModal2
          shouldShow={showNodeToAddModal}
          onRequestClose={() => {
            setShowNodeToAddModal(false);
            //setNodeDataToEdit(undefined);
          }}
          node={'nodeToEdit'}
          vertexCollections={vertexCollections}
          nodeData={{}}
          editorContent={'nodeToEdit'}
          onAddNode={() => {
            //setGraphData(newGraphData);
            setShowNodeToAddModal(false);
          }}
          //nodeKey={'Munich'}
          //nodeCollection={'germanCity'}
          nodeKey={''}
          nodeCollection={''}
          onNodeCreation={(newNode) => {
            console.log("newNode in G6JsGraph: ", newNode);
            updateGraphDataWithNode(newNode)
          }}
          graphName={graphName}
          graphData={graphData}
        >
          <strong>Add node</strong>
        </AddNodeModal2>
        

        <GraphView
              data={graphData}
              vertexCollections={vertexCollections}
              onUpdateNodeGraphData={(newGraphData) => updateGraphDataNodes(newGraphData)}
              onUpdateEdgeGraphData={(newGraphData) => updateGraphDataEdges(newGraphData)}
              onAddSingleNode={(newNode) => updateGraphDataWithNode(newNode)}
              onAddSingleEdge={(newEdge) => {
                console.log("onAddSingleEdge (newEdge): ", newEdge);
                setEdgeModelToAdd(newEdge);
                openAddEdgeModal(newEdge);
                //updateGraphDataWithEdge(newEdge)
              }}
              onRemoveSingleNode={(node) => removeNode(node)}
              onRemoveSingleEdge={(edge) => removeEdge(edge)}
              onEditNode={(node) => openEditModal(node)}
              onEditEdge={(edge) => openEditEdgeModal(edge)}
              onAddNodeToDb={() => openAddNodeModal()}
              onExpandNode={(node) => expandNode(node)}
              onSetStartnode={(node) => setStartnode(node)}
              onGraphSending={(drawnGraph) => receiveDrawnGraph(drawnGraph)}
              graphName={graphName}
              responseDuration={responseTimes.fetchDuration}
              onChangeGraphData={(newGraphData) => setGraphData(newGraphData)}
              onClickDocument={(document) => lookUpDocument(document)}
              onLoadFullGraph={() => setShowFetchFullGraphModal(true)}
              onGraphDataLoaded={(newGraphData) => {
                //nodeColorAttribute
                console.log("newGraphData after fetching with nodeColorAttribute: ", newGraphData);
                if(newGraphData.settings.nodesColorAttributes) {
                  setNodesColorAttributes(newGraphData.settings.nodesColorAttributes);
                  console.log("newGraphData.settings.nodesColorAttributes: ", newGraphData.settings.nodesColorAttributes);
                }
                if(newGraphData.settings.nodesSizeMinMax) {
                  setNodesSizeMinMax(newGraphData.settings.nodesSizeMinMax);
                }
                if(newGraphData.settings.connectionsMinMax) {
                  setConnectionsMinMax(newGraphData.settings.connectionsMinMax);

                  /*
                  if(typeof connectionsMinMax == "undefined") {
                    console.error("connectionsMinMax is undefined");
                    setConnectionsMinMax(newGraphData.settings.connectionsMinMax);
                  }
                  
                  if(typeof connectionsMinMax != "undefined") {
                    console.log("(HERE) newGraphData.settings.connectionsMinMax[0]: ", newGraphData.settings.connectionsMinMax[0]);
                    console.log("(HERE) connectionsMinMax[0]: ", connectionsMinMax[0]);
                    if(newGraphData.settings.connectionsMinMax[0] < connectionsMinMax[0]) {
                      console.error("(HERE) The minimum of edgescount changed from " + connectionsMinMax[0] + " to " + newGraphData.settings.connectionsMinMax[0]);
                      setConnectionsMinMax([newGraphData.settings.connectionsMinMax[0], connectionsMinMax[1]]);
                    }
                  }
                  if(typeof connectionsMinMax != "undefined") {
                    console.log("(HERE) newGraphData.settings.connectionsMinMax[1]: ", newGraphData.settings.connectionsMinMax[1]);
                    console.log("(HERE) connectionsMinMax[1]: ", connectionsMinMax[1]);
                    if(newGraphData.settings.connectionsMinMax[1] > connectionsMinMax[1]) {
                      console.error("(HERE) The maximum of edgescount changed from " + connectionsMinMax[1] + " to " + newGraphData.settings.connectionsMinMax[1]);
                      setConnectionsMinMax([connectionsMinMax[0], newGraphData.settings.connectionsMinMax[1]]);
                    }
                  }
                  */
                  
                  //setConnectionsMinMax(newGraphData.settings.connectionsMinMax);
                }
                if(newGraphData.settings.edgesColorAttributes) {
                  setEdgesColorAttributes(newGraphData.settings.edgesColorAttributes);
                  console.log("newGraphData.settings.edgesColorAttributes: ", newGraphData.settings.edgesColorAttributes);
                }
                setGraphData(newGraphData);
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
        <AttributesInfo attributes={lookedUpData} />
        <ButtonScrollTo />
      </UrlParametersContext.Provider>
    </div>
  );
}

export default G6JsGraph;
