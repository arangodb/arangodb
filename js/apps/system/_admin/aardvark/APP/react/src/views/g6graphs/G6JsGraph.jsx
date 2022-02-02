/* global arangoHelper, arangoFetch, frontendConfig */

import React, { useCallback, useEffect, useState } from 'react';
import ReactDOM from 'react-dom';
import { GraphView } from './GraphView';
import { data } from './data';
import { data2 } from './data2';
import G6 from '@antv/g6';
import { Card  } from 'antd';
import NodeStyleSelector from './NodeStyleSelector.js';
import { NodeList } from './components/node-list/node-list.component';
import { EdgeList } from './components/edge-list/edge-list.component';
import EdgeStyleSelector from './EdgeStyleSelector.js';
import NodeLabelContentSelector from './NodeLabelContentSelector.js';
import { EditNodeModal } from './EditNodeModal';
import { Headerinfo } from './Headerinfo';
import { LoadDocument } from './LoadDocument';
import { EditModal } from './EditModal';
import { EditEdgeModal } from './EditEdgeModal';
import { AddNodeModal2 } from './AddNodeModal2';
import AqlEditor from './AqlEditor';
import { SlideInMenu } from './SlideInMenu';
import omit from "lodash";
import { JsonEditor as Editor } from 'jsoneditor-react';
import './tooltip.css';

const G6JsGraph = () => {
  let responseTimesObject = {
    fetchStarted: null,
    fetchFinished: null,
    fetchDuration: null
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
  const [showEditNodeModal, setShowEditNodeModal] = useState(false);
  const [showEditModal, setShowEditModal] = useState(false);
  const [showEditEdgeModal, setShowEditEdgeModal] = useState(false);
  const [editNode, setEditNode] = useState();
  const [nodeToEdit, setNodeToEdit] = useState(
  {
    "keys": [
      "Hamburg"
    ],
    "collection": "germanCity"
  });
  const [edgeToEdit, setEdgeToEdit] = useState(
    {
      "keys": [
        "Hamburg"
      ],
      "collection": "germanCity"
    });
  const [nodeDataToEdit, setNodeDataToEdit] = useState();
  const [edgeDataToEdit, setEdgeDataToEdit] = useState();
  const [nodeKey, setNodeKey] = useState('Hamburg');
  const [edgeKey, setEdgeKey] = useState('Hamburg');
  const [nodeCollection, setNodeCollection] = useState('germanCity');
  const [edgeCollection, setEdgeCollection] = useState('germanCity');
  const [selectedNodeData, setSelectedNodeData] = useState();
  const [nodeToAdd, setNodeToAdd] = useState();
  const [nodeToAddKey, setNodeToAddKey] = useState("Munich");
  const [nodeToAddCollection, setNodeToAddCollection] = useState("germanCity");
  const [nodeToAddData, setNodeToAddData] = useState();
  const [isNewNodeToEdit, setIsNewNodeToEdit] = useState(false);
  const [showNodeToAddModal, setShowNodeToAddModal] = useState();
  const [type, setLayout] = React.useState('graphin-force');
  let [aqlQueryString, setAqlQueryString] = useState("/_admin/aardvark/g6graph/routeplanner");
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
    console.log(">>>>>>>>>>>> openEditModal (node): ", node);
    const slashPos = node.indexOf("/");
    const label = node.substring(slashPos + 1) + " - " + node.substring(0, slashPos);
    const data = [{
      "keys": [
        "Berlin"
      ],
      "collection": "germanCity"
    }];
    setNodeToEdit(node);
    //setNodeKey('Paris');
    console.log(">>>>>>>>>>>> openEditModal (nodeKey): ", node.substring(slashPos + 1));
    setNodeKey(node.substring(slashPos + 1));
    //setNodeCollection('frenchCity');
    console.log(">>>>>>>>>>>> openEditModal (nodeCollection): ", node.substring(0, slashPos));
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
        console.log("Document info loaded before opening EditModal component: ");
        console.log("Document info loaded before opening EditModal component (data): ", data);
        console.log("Document info loaded before opening EditModal component (data.documents): ", data.documents);
        console.log("Document info loaded before opening EditModal component (data.documents[0]): ", data.documents[0]);
        const allowedDocuments = omit(data.documents[0], '_id', '_key');
        console.log("allowedDocuments: ", allowedDocuments);
        setNodeDataToEdit(data.documents[0]);
        //setNodeDataToEdit(data);
      })
      .catch((err) => {
        console.log(err);
      });
    console.log("openEditModal: change nodeToEdit now");
    setIsNewNodeToEdit(true);
    //return <LoadDocument node={node} />
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
        const allowedDocuments = omit(data.documents[0], '_id', '_key');
        console.log("allowedDocuments: ", allowedDocuments);
        setEdgeDataToEdit(data.documents[0]);
      })
      .catch((err) => {
        console.log(err);
      });
    console.log("openEditEdgeModal: change edgeToEdit now");

    setShowEditEdgeModal(true);
  }

  const openAddNodeModal = () => {
    console.log(">>>>>>>>>>>> openAddNodeModal");
    const data = [{
      "keys": [
        "Munich"
      ],
      "collection": "germanCity"
    }];
    setNodeToAdd(data);
    setNodeToAddKey("Munich");
    setNodeToAddCollection("germanCity");
    const nodeToAddDataObject = {
      "keys": [
        "Munich"
      ],
      "collection": "germanCity"
    };
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
    console.log("openEditModal: change nodeToEdit now");
    //return <LoadDocument node={node} />
    setShowNodeToAddModal(true);
  }

  const expandNode = (node) => {
    console.log(">>>>>>>>>>>> expandNode (node): ", node);
    const url = `/_admin/aardvark/graph/${graphName}?depth=2&limit=250&nodeColor=#2ecc71&nodeColorAttribute=&nodeColorByCollection=true&edgeColor=#cccccc&edgeColorAttribute=&edgeColorByCollection=false&nodeLabel=_key&edgeLabel=&nodeSize=&nodeSizeByEdges=true&edgeEditable=true&nodeLabelByCollection=false&edgeLabelByCollection=false&nodeStart=&barnesHutOptimize=true&query=FOR v, e, p IN 1..1 ANY "${node}" GRAPH "${graphName}" RETURN p`;
      arangoFetch(arangoHelper.databaseUrl(url), {
        method: "GET"
      })
      .then(response => response.json())
      .then(data => {
        console.log("Received expanded garph data: ", data);
        setGraphData(data);
      })
      .catch((err) => {
        console.log(err);
      });
    //setShowNodeToAddModal(true);
  }

  const setStartnode = (node) => {
    const url = `/_admin/aardvark/graph/${graphName}?nodeLabelByCollection=true&nodeColorByCollection=false&nodeSizeByEdges=true&edgeLabelByCollection=true&edgeColorByCollection=false&nodeStart=${node}&depth=1&limit=2&nodeLabel=&nodeColor=#2ecc71&nodeColorAttribute=&nodeSize=&edgeLabel=&edgeColor=#cccccc&edgeColorAttribute=&edgeEditable=true`;
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
      <button onClick={() => printGraphData()}>Print graphData</button>
      <button onClick={() => openEditNodeModal()}>Open edit node modal</button>
      <button onClick={() => openEditModal()}>Open edit modal</button>
      <LayoutSelector options={layouts} value={type} onChange={handleChange} />
      <Headerinfo
        graphName={graphName}
        graphData={graphData}
        responseDuration={responseTimes.fetchDuration}
      />
      */

  return (
    <div>
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
      <EditModal
        shouldShow={showEditModal}
        onRequestClose={() => {
          setShowEditModal(false);
          setNodeDataToEdit(undefined);
        }}
        node={nodeToEdit}
        nodeData={nodeDataToEdit}
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
      <AddNodeModal2
        shouldShow={showNodeToAddModal}
        onRequestClose={() => {
          setShowNodeToAddModal(false);
          //setNodeDataToEdit(undefined);
        }}
        node={'nodeToEdit'}
        nodeData={[]}
        editorContent={'nodeToEdit'}
        onAddNode={() => {
          //setGraphData(newGraphData);
          setShowNodeToAddModal(false);
        }}
        nodeKey={'Munich'}
        nodeCollection={'germanCity'}
        onNodeCreation={(newNode) => updateGraphDataWithNode(newNode)}
      >
        <strong>Add node</strong>
      </AddNodeModal2>
      
      <GraphView
            data={graphData}
            onUpdateNodeGraphData={(newGraphData) => updateGraphDataNodes(newGraphData)}
            onUpdateEdgeGraphData={(newGraphData) => updateGraphDataEdges(newGraphData)}
            onAddSingleNode={(newNode) => updateGraphDataWithNode(newNode)}
            onAddSingleEdge={(newEdge) => updateGraphDataWithEdge(newEdge)}
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
      />     
    </div>
  );
}

export default G6JsGraph;
