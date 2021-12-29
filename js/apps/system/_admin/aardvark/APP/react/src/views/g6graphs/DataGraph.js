/* global arangoHelper, arangoFetch, frontendConfig */

import React, { useEffect, useState, useCallback, useRef } from 'react';
import Graphin, { Utils, GraphinContext } from '@antv/graphin';
import Modal, { ModalBody, ModalFooter, ModalHeader } from '../../components/modal/Modal';
import { message, Card, Select } from 'antd';
import { ContextMenu, MiniMap, Toolbar } from '@antv/graphin-components';
import { useFetch } from './useFetch';
import { Cell, Grid } from "../../components/pure-css/grid";
import AqlEditor from './AqlEditor';
import CollectionLoader from './CollectionLoader';
import {
  TagFilled,
  DeleteFilled,
  ExpandAltOutlined,
  TrademarkCircleFilled,
  ChromeFilled,
  BranchesOutlined,
  ApartmentOutlined,
  AppstoreFilled,
  CopyrightCircleFilled,
  CustomerServiceFilled,
  ShareAltOutlined,
  EditOutlined,
  QuestionCircleOutlined
} from '@ant-design/icons';
import { NodeList } from './components/node-list/node-list.component';
import { EdgeList } from './components/edge-list/edge-list.component';
import { EditNodeModal } from './EditNodeModal';
import { DeleteNodeModal } from './DeleteNodeModal';
import { AddNodeModal } from './AddNodeModal';
import { G6GraphHelpModal } from './G6GraphHelpModal';
import { ControlledModal } from './ControlledModal';
import { GraphDataChanger } from './GraphDataChanger';
import { DeleteNodeFromGraphData } from './DeleteNodeFromGraphData';
import { AddNodeToGraphData } from './AddNodeToGraphData';
import { AddEdgeToGraphData } from './AddEdgeToGraphData';
import './dataGraphStyles.css';

const GraphDataInfo = (graphData) => {
  return (
    <>
      <span className="badge badge-info pull-left" style={{"marginTop": "12px" }}>{graphData.graphData.nodes.length} nodes</span>
      <span className="badge badge-info pull-right" style={{"marginTop": "12px" }}>{graphData.graphData.edges.length} edges</span>
    </>
  );
}

const DataGraph = () => {
  let [queryString, setQueryString] = useState("/_admin/aardvark/g6graph/routeplanner");
  let [aqlQueryString, setAqlQueryString] = useState("");
  let [graphName, setGraphName] = useState("routeplanner");
  let [graphData, setGraphData] = useState(null);
  let [graphDataNodes, setGraphDataNodes] = useState([]);
  let [graphNodes, setGraphNodes] = useState([]);
  const [changeCount, setChangeCount] = useState(0);
  let [queryMethod, setQueryMethod] = useState("GET");
  const [editNodeModalVisibility, setEditNodeModalVisibility] = useState(false);
  const [toggleValue, setToggleValue] = useState(true);
  const [currentNode, setCurrentNode] = useState("node1");
  const [deleteNodeModalShow, setDeleteNodeModalShow] = useState(false);
  const [showDeleteNodeModal, setShowDeleteNodeModal] = useState(false);
  const [showAddNodeModal, setShowAddNodeModal] = useState(false);
  const [nodeId, setNodeId] = useState('Vancouver');
  const [deleteNodeId, setDeletNodeId] = useState("");
  const [editNode, setEditNode] = useState();
  const [showEditNodeModal, setShowEditNodeModal] = useState(false);

  const iconMap = {
    'graphin-force': <ShareAltOutlined />,
    random: <TrademarkCircleFilled />,
    concentric: <ChromeFilled />,
    circle: <BranchesOutlined />,
    force: <AppstoreFilled />,
    dagre: <ApartmentOutlined />,
    grid: <CopyrightCircleFilled />,
    radial: <ShareAltOutlined />,
  };
  const layouts = [
    { type: 'graphin-force' },
    {
        type: 'grid',
    },
    {
        type: 'circular',
    },
    {
        type: 'radial',
    },
    {
        type: 'force',
        preventOverlap: true,
        linkDistance: 50,
        nodeStrength: 30,
        edgeStrength: 0.8,
        collideStrength: 0.8,
        nodeSize: 30,
        alpha: 0.9,
        alphaDecay: 0.3,
        alphaMin: 0.01,
        forceSimulation: null,
        onTick: () => {
            console.log('ticking');
        },
        onLayoutEnd: () => {
            console.log('force layout done');
        },
    },
    {
        type: 'gForce',
        linkDistance: 150,
        nodeStrength: 30,
        edgeStrength: 0.1,
        nodeSize: 30,
        onTick: () => {
            console.log('ticking');
        },
        onLayoutEnd: () => {
            console.log('force layout done');
        },
        workerEnabled: false,
        gpuEnabled: false,
    },
    {
        type: 'concentric',
        maxLevelDiff: 0.5,
        sortBy: 'degree',
    },
    {
        type: 'dagre',
        rankdir: 'LR',
    },
    {
        type: 'fruchterman',
    },
    {
        type: 'mds',
        workerEnabled: false,
    },
    {
        type: 'comboForce',
    },
  ];

  // fetching data # start
  const fetchData = useCallback(() => {
    arangoFetch(arangoHelper.databaseUrl(queryString), {
      method: queryMethod,
    })
    .then(response => response.json())
    .then(data => {
      console.log("NEW DATA");
      console.log(data);
      
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

  // diverse functions # start
  const toggleEditNodeModal = (editNodeModalVisibility) => {
    setEditNodeModalVisibility(editNodeModalVisibility);
  };
  // diverse functions # end

  // Component LayoutSelector # start
  const SelectOption = Select.Option;
  const LayoutSelector = props => {
    const { value, onChange, options } = props;

    return (
      <div>
        <Select style={{ width: '90%' }} value={value} onChange={onChange}>
          {options.map(item => {
            const { type } = item;
            const iconComponent = iconMap[type] || <CustomerServiceFilled />;
            return (
              <SelectOption key={type} value={type}>
                {iconComponent} &nbsp;
                {type}
              </SelectOption>
            );
          })}
        </Select>
      </div>
    );
  };
  const [type, setLayout] = React.useState('graphin-force');
  const handleChange = value => {
    console.log('value', value);
    setLayout(value);
  };
  const layout = layouts.find(item => item.type === type);
  // Component LayoutSelector # end

  // Component fetchAqlQueryData # start
  const fetchAqlQueryData = useCallback(() => {
    // http://localhost:8529/_db/_system/_admin/aardvark/graph/routeplanner?nodeLabelByCollection=false&nodeColorByCollection=true&nodeSizeByEdges=true&edgeLabelByCollection=false&edgeColorByCollection=false&nodeStart=frenchCity/Paris&depth=1&limit=250&nodeLabel=_key&nodeColor=#2ecc71&nodeColorAttribute=&nodeSize=&edgeLabel=&edgeColor=#cccccc&edgeColorAttribute=&edgeEditable=true
    arangoFetch(arangoHelper.databaseUrl(aqlQueryString), {
      method: 'GET',
    })
    .then(response => response.json())
    .then(data => {
      console.log("NEW DATA from aqlQuery");
      console.log(data);
      
      setGraphData(data);
    })
    .catch((err) => {
      console.log(err);
    });
  }, [aqlQueryString]);

  useEffect(() => {
    fetchAqlQueryData();
  }, [fetchAqlQueryData]);
  // Component fetchAqlQueryData # end

  const changeGraphNodes = useCallback((nodes) => {
      console.log("changeGraphNodes() triggered: ", graphNodes);
      console.log("changeGraphNodes() given nodes: ", nodes);
      console.log("stringified nodesinnput: ", JSON.stringify(nodes));
      console.log("current graphData: ", graphData);
      if(nodes === "ja") {
        const limitedNodes = [
          {
            id: "frenchCity/Paris",
            label: "Paris",
            style: {
              label: {
                value: "Paris"
              }
            }
          },
          {
            id: "germanCity/Berlin",
            label: "Berlin",
            style: {
              label: {
                value: "Berlin"
              }
            }
          },
          {
            id: "germanCity/Hamburg",
            label: "Hamburg",
            style: {
              label: {
                value: "Hamburg"
              }
            }
          },
          {
            id: "germanCity/Cologne",
            label: "Cologne",
            style: {
              label: {
                value: "Cologne"
              }
            }
          }
        ];
        setGraphNodes(limitedNodes);
        setQueryString(`/_admin/aardvark/g6graph/social`);
      }
  }, [graphNodes]);

  useEffect(() => {
    changeGraphNodes();
  }, [changeGraphNodes]);

  // fetchNewNodes component - start
  const fetchNewNodes = useCallback(() => {

    const limitedNodes = [
      {
        id: "frenchCity/Paris",
        label: "Paris",
        style: {
          label: {
            value: "Paris"
          }
        }
      },
      {
        id: "germanCity/Berlin",
        label: "Berlin",
        style: {
          label: {
            value: "Berlin"
          }
        }
      },
      {
        id: "germanCity/Hamburg",
        label: "Hamburg",
        style: {
          label: {
            value: "Hamburg"
          }
        }
      },
      {
        id: "germanCity/Cologne",
        label: "Cologne",
        style: {
          label: {
            value: "Cologne"
          }
        }
      }
    ];

    const tempGraphData = graphData;
    console.log('tempGraphData (1): ', tempGraphData);
    tempGraphData.nodes = limitedNodes;
    console.log('tempGraphData (2): ', tempGraphData);
    setGraphData(tempGraphData);
  }, [graphDataNodes]);
  // fetchData component - end

  /*
  useEffect(() => {
    fetchNewNodes();
  }, [fetchNewNodes]);
  */

  // change graphData
  const changeGraphData = useCallback((newGraphData) => {
    setGraphData(newGraphData);
  }, [changeCount]);

  useEffect(() => {
    changeGraphData();
  }, [changeGraphData]);

  

  // Menu Component
  const { Menu } = ContextMenu;
  const menuOptions = [
        {
            key: 'tag',
            icon: <TagFilled />,
            name: 'Set as startnode',
        },
        {
            key: 'delete',
            icon: <DeleteFilled />,
            name: 'Delete',
        },
        {
            key: 'expand',
            icon: <ExpandAltOutlined />,
            name: 'Expand',
        },
        {
          key: 'editnode',
          icon: <EditOutlined />,
          name: 'Edit',
        },
  ];

  const searchChange = e => {
    this.setState({ searchField: e.target.value });
  }
  // Sektor C - 1. Rang, C1 - C, Reihe 11, Platz 80
  
  const menuHandleChange = (menuItem, menuData, graphData) => {
      console.log("menuHandleChange -> menuItem: ", menuData);
      console.log("menuHandleChange -> menuData: ", menuData);
      console.log("menuHandleChange -> graphData: ", graphData);

      const reallyDeleteModal = (nodeId) => {
        console.log("nodeId in menuHandleChange: ", nodeId.id);
        setDeletNodeId(nodeId.id);
        setShowDeleteNodeModal(!showDeleteNodeModal);
      }

      const addNodeModal = () => {
        console.log("addNodeModal()");
        setShowAddNodeModal(!showAddNodeModal);
      }

      const editModal = (nodeId) => {
        console.log("nodeId in menuHandleChange (editModal): ", nodeId.id);
        console.log("node object in menuHandleChange (editModal): ", nodeId);
        //setEditNode(nodeId);
        setEditNode(nodeId.id);
        //setShowEditNodeModal(!showEditNodeModal);
        setShowEditNodeModal(true);
      }

      //message.info(`Element：${menuData.id}，Action：${menuItem.name}, Key：${menuItem.key}`);
      let query;
      switch(menuItem.key) {
        case "tag":
            message.info(`GraphData：${graphData}`);
            query = '/_admin/aardvark/g6graph/social';
            queryMethod = "GET";
            message.info(`query: ${query}; queryMethod: ${queryMethod}`);
            setQueryMethod(queryMethod);
            setQueryString(query);
            break;
        case "delete":
            reallyDeleteModal(menuData);
            /*
            query = `/_api/gharial/routeplanner/vertex/${menuData.id}`;
            queryMethod = "DELETE";
            message.info(`query: ${query}; queryMethod: ${queryMethod}`);
            setQueryMethod(queryMethod);
            setQueryString(query);
            */
            break;
        case "expand":
            message.info(`GraphData：${graphData}`);
            query = `/_admin/aardvark/graph/routeplanner?depth=2&limit=250&nodeColor=#2ecc71&nodeColorAttribute=&nodeColorByCollection=true&edgeColor=#cccccc&edgeColorAttribute=&edgeColorByCollection=false&nodeLabel=_key&edgeLabel=&nodeSize=&nodeSizeByEdges=true&edgeEditable=true&nodeLabelByCollection=false&edgeLabelByCollection=false&nodeStart=&barnesHutOptimize=true&query=FOR v, e, p IN 1..1 ANY "${menuData.id}" GRAPH "routeplanner" RETURN p`;
            queryMethod = "GET";
            message.info(`query: ${query}; queryMethod: ${queryMethod}`);
            setQueryMethod(queryMethod);
            setQueryString(query);
            break;
        case "editnode":
            //message.info(`GraphData：${graphData}`);
            //query = `/_admin/aardvark/graph/routeplanner?depth=2&limit=250&nodeColor=#2ecc71&nodeColorAttribute=&nodeColorByCollection=true&edgeColor=#cccccc&edgeColorAttribute=&edgeColorByCollection=false&nodeLabel=_key&edgeLabel=&nodeSize=&nodeSizeByEdges=true&edgeEditable=true&nodeLabelByCollection=false&edgeLabelByCollection=false&nodeStart=&barnesHutOptimize=true&query=FOR v, e, p IN 1..1 ANY "${menuData.id}" GRAPH "routeplanner" RETURN p`;
            //queryMethod = "GET";
            editModal(menuData);
            //message.info(`Open Modal with text editor`);
            //toggleEditNodeModal(true);
            //showEditNodeModal(graphData);
            break;
        default:
            break;
      }
  };

  const onDeleteNode = (nodeId) => {
    console.log('nodeId in onDeleteNode in MenuGraph: ', nodeId);
    console.log('graphData in onDeleteNode in MenuGraph (1): ', graphData);
    /*
    const newGraphData = graphData.nodes.filter((node) => {
      return node.id !== nodeId;
    });
    console.log('newGraphData in onDeleteNode in MenuGraph: ', newGraphData);
    */
    //setGraphData(newGraphData);
    console.log('graphData in onDeleteNode in MenuGraph after delete (2): ', graphData);

    return <>
      <Modal show={true} setShow={true}>
        <ModalHeader title={`Delete node ${nodeId}`}/>
        <ModalBody>
          <dl>
            <dt>Delet node</dt>
            <dd>
              If you are new to graphs and displaying graph data... have a look at our <strong><a target={'_blank'} rel={'noreferrer'} href={'https://www.arangodb.com/docs/stable/graphs.html'}>graph documentation</a></strong>.
            </dd>
            <dt>Graph Course</dt>
            <dd>
              We also offer a course with the title <strong><a target={'_blank'} rel={'noreferrer'} href={'https://www.arangodb.com/learn/graphs/graph-course/'}>Get Started with Graphs in ArangoDB</a></strong> for free.
            </dd>
            <dt>Quick Start</dt>
            <dd>
              To start immediately, the explanationn of our <strong><a target={'_blank'} rel={'noreferrer'} href={'https://www.arangodb.com/docs/stable/graphs.html#example-graphs'}>example graphs</a></strong> might help very well.
            </dd>
          </dl>
        </ModalBody>
        <ModalFooter>
          <button className="button-close" onClick={() => console.log('Options')}>Close</button>
        </ModalFooter>
      </Modal>
    </>;
  };

  const onAddNode = (newGraphData) => {
    console.log("DataGraph in onAddNode (newGraphData): ", newGraphData);
  }

  const graphHelpModalRef = useRef();
  const deleteNodeModalRef = useRef();

  const masterFunc = (name) => {
    const randomNumber = Math.floor(Math.random() * 100);
    console.log("currentNode (1): ", currentNode);
    setCurrentNode(randomNumber);
    console.log("currentNode (2): ", currentNode);
  };

  useEffect(() => {
    console.log(`currentNode changed to ${currentNode}`);
  }, [currentNode]);

  const toggleDeleteNodeModal = () => {
    setCurrentNode('Washington');
    console.log('deleteNodeModalShow (1): ', deleteNodeModalShow);
    setDeleteNodeModalShow(!deleteNodeModalShow);
    console.log('deleteNodeModalShow (2): ', deleteNodeModalShow);
    console.log('currentNode: ', currentNode);
  }

  /*
  function removeNode(id) {
    let limitedNodes = nodes.filter((node) => node.id !== id);
    return limitedNodes;
  }
  */

  const RequestDelete = (nodeId) => {
    //let limitedNodes = nodes.filter((node) => node.id !== id);
    //return limitedNodes;
    const limitedNodes = [
      {
        id: "frenchCity/Paris",
        label: "Paris",
        style: {
          label: {
            value: "Paris"
          }
        }
      },
      {
        id: "germanCity/Berlin",
        label: "Berlin",
        style: {
          label: {
            value: "Berlin"
          }
        }
      },
      {
        id: "germanCity/Hamburg",
        label: "Hamburg",
        style: {
          label: {
            value: "Hamburg"
          }
        }
      },
      {
        id: "germanCity/Cologne",
        label: "Cologne",
        style: {
          label: {
            value: "Cologne"
          }
        }
      }
    ];
    graphData.nodes = limitedNodes;
    changeGraphData(graphData);
  }
  // /_admin/aardvark/g6graph/social
  // /_admin/aardvark/g6graph/routeplanner

  const changeNodes = (nodes) => {
    const limitedNodes = [
      {
        id: "frenchCity/Paris",
        label: "Paris",
        style: {
          label: {
            value: "Paris"
          }
        }
      },
      {
        id: "germanCity/Berlin",
        label: "Berlin",
        style: {
          label: {
            value: "Berlin"
          }
        }
      },
      {
        id: "germanCity/Hamburg",
        label: "Hamburg",
        style: {
          label: {
            value: "Hamburg"
          }
        }
      },
      {
        id: "germanCity/Cologne",
        label: "Cologne",
        style: {
          label: {
            value: "Cologne"
          }
        }
      }
    ];
    setGraphDataNodes(limitedNodes);
    console.log('changeNodes (nodes): ', nodes);
    console.log('changeNodes (limitedNodes): ', limitedNodes);
  }

  const changeCollection = (myString) => {
    console.log('in changeCollection');
    //http://localhost:8529/_db/_system/_admin/aardvark/g6graph/social
    //http://localhost:8529/_db/_system/aardvark/g6graph/social
    setQueryString(`/_admin/aardvark/g6graph/${myString}`);
  }

  if(graphData) {
    console.log("graphdata (nodes) in menu graph");
    console.log(graphData.nodes);
    console.log("graphdata (edges) in menu graph");
    console.log(graphData.edges);
    return (
      <div>
        <Card
          title={ `G6 Graph Viewer (loaded graph: ${graphName})` }
          extra={
            <>
              <Grid>
                <Cell size={'1-4'}>
                  <CollectionLoader onLoadCollection={(graphName) => {
                    changeCollection(graphName);
                    setGraphName(graphName);}}
                  />
                  <hr style={{width: '90%'}} />
                  <AqlEditor
                    queryString={queryString}
                    onNewSearch={(myString) => {setQueryString(myString)}}
                    onQueryChange={(myString) => {setQueryString(myString)}}
                    onOnclickHandler={(myString) => changeCollection(myString)}
                    onReduceNodes={(nodes) => changeGraphNodes(nodes)}
                    onAqlQueryHandler={(myString) => setAqlQueryString(myString)}
                  />
                </Cell>
                <Cell size={'1-4'}>
                  <LayoutSelector options={layouts} value={type} onChange={handleChange} />
                  <NodeList
                  nodes={graphData.nodes}
                  graphData={graphData}
                  onNodeInfo={() => console.log('onNodeInfo() in MenuGraph')}
                  onDeleteNode={onDeleteNode} />
                  <EdgeList edges={graphData.edges} />
                  <GraphDataInfo graphData={graphData}/>
                </Cell>
                <Cell size={'1-4'}>
                  <AddNodeToGraphData
                    graphData={graphData}
                    onAddNode={(newGraphData) => {
                      console.log('newGraphData in addNode: ', newGraphData);
                      setGraphData(newGraphData);
                      //setShowDeleteNodeModal(false);
                    }}
                  />
                </Cell>
                <Cell size={'1-4'}>
                  <AddEdgeToGraphData
                    graphData={graphData}
                    onAddEdge={(newGraphData) => {
                      console.log('newGraphData in addEdge: ', newGraphData);
                      setGraphData(newGraphData);
                    }}
                  />
                </Cell>
              </Grid>
              <AddNodeModal
                graphData={graphData}
                shouldShow={showAddNodeModal}
                onRequestClose={() => {
                  setShowAddNodeModal(false);
                }}
                onAddNode={(newGraphData) => {
                  console.log('newGraphData in onAddNode: ', newGraphData);
                  setGraphData(newGraphData);
                  setShowAddNodeModal(false);
                }}
              >
                <h1>Add Node</h1>
              </AddNodeModal>
              <DeleteNodeModal
                graphData={graphData}
                shouldShow={showDeleteNodeModal}
                onRequestClose={() => {
                  setShowDeleteNodeModal(false);
                }}
                nodeId={deleteNodeId}
                onDeleteNode={(newGraphData) => {
                  console.log('newGraphData in DeleteNode: ', newGraphData);
                  setGraphData(newGraphData);
                  setShowDeleteNodeModal(false);
                }}
              >
                <h1>Delete Node</h1>
                <p>Really delete node: {deleteNodeId}</p>
              </DeleteNodeModal>
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
                <h1>Edit Node</h1>
                <p>Really edit node: {editNode}</p>
              </EditNodeModal>
            </>
          }
        >
          <Graphin data={graphData} layout={layout}>
            <ContextMenu>
              <Menu options={menuOptions} onChange={menuHandleChange} graphData={graphData} bindType="node" />
            </ContextMenu>
          </Graphin>
        </Card>
      </div>
    );
  } else {
    return null;
  }
}

export default DataGraph;
