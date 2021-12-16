/* global arangoHelper, arangoFetch, frontendConfig */

import React, { useEffect, useState, useCallback, useRef } from 'react';
import Graphin, { Utils, GraphinContext } from '@antv/graphin';
import Modal, { ModalBody, ModalFooter, ModalHeader } from '../../components/modal/Modal';
import { message, Card, Select } from 'antd';
import { ContextMenu, MiniMap, Toolbar } from '@antv/graphin-components';
import { useFetch } from './useFetch';
import AqlEditor from './AqlEditor';
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
import { EditNodeModal } from './components/modals/EditNodeModal';
import { DeleteNodeModal } from './DeleteNodeModal';
import { G6GraphHelpModal } from './G6GraphHelpModal';

const GraphDataInfo = (graphData) => {
  return (
    <>
      <span className="badge badge-info pull-left" style={{"marginTop": "12px" }}>{graphData.graphData.nodes.length} nodes</span>
      <span className="badge badge-info pull-right" style={{"marginTop": "12px" }}>{graphData.graphData.edges.length} edges</span>
    </>
  );
}

const MenuGraph = () => {
  let [queryString, setQueryString] = useState("/_admin/aardvark/g6graph/routeplanner");
  let [graphData, setGraphData] = useState(null);
  let [queryMethod, setQueryMethod] = useState("GET");
  const [editNodeModalVisibility, setEditNodeModalVisibility] = useState(false);
  const [toggleValue, setToggleValue] = useState(true);
  const [currentNode, setCurrentNode] = useState("node1");
  const [deleteNodeModalShow, setDeleteNodeModalShow] = useState(false);

  const toggleModal = (toggleValue) => {
    console.log('toggleValue', toggleValue);
    setToggleValue(toggleValue);
  };

  const TestModal = (visibility) => {
    const [show, setShow] = useState(false);
  
    const showTestModal = (event) => {
      event.preventDefault();
      setShow(visibility);
    };
  
    return <>
      <a href={'#g6'} onClick={showTestModal}>
        <QuestionCircleOutlined /> TestModal
      </a>
      <Modal show={show} setShow={setShow}>
        <ModalHeader title={'TestModal'}/>
        <ModalBody>
          <dl>
            <dt>Graphs</dt>
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
          <button className="button-close" onClick={() => setShow(false)}>Close</button>
        </ModalFooter>
      </Modal>
    </>;
  };

  const toggleEditNodeModal = (editNodeModalVisibility) => {
    setEditNodeModalVisibility(editNodeModalVisibility);
  };

  const showDeleteNodeModal = (menuData, graphData) => {
    message.info(`showDeleteNodeModal: Open Modal with text editor (${menuData.id})`);
    console.log("menuData: ", menuData);
    console.log("graphData: ", graphData);
  }

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

  const showEditNodeModal = (node) => {
    message.info(`showEditNodeModal: Open Modal with text editor (${node})`);
    console.log("MenuGraph: showEditNodeModal", node);
  }

  const SelectOption = Select.Option;
  const LayoutSelector = props => {
    const { value, onChange, options } = props;

    return (
      <div>
        <Select style={{ width: '100%' }} value={value} onChange={onChange}>
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

  const layouts = [
    { type: 'graphin-force' },
    {
        type: 'grid',
        // begin: [0, 0], // optional，
        // preventOverlap: true, // optional
        // preventOverlapPdding: 20, // optional
        // nodeSize: 30, // optional
        // condense: false, // optional
        // rows: 5, // optional
        // cols: 5, // optional
        // sortBy: 'degree', // optional
        // workerEnabled: false, // optional
    },
    {
        type: 'circular',
        // center: [200, 200], // optional
        // radius: null, // optional
        // startRadius: 10, // optional
        // endRadius: 100, // optional
        // clockwise: false, // optional
        // divisions: 5, // optional
        // ordering: 'degree', // optional
        // angleRatio: 1, // optional
    },
    {
        type: 'radial',
        // center: [200, 200], // optional
        // linkDistance: 50, // optional，
        // maxIteration: 1000, // optional
        // focusNode: 'node11', // optional
        // unitRadius: 100, // optional
        // preventOverlap: true, // optional
        // nodeSize: 30, // optional
        // strictRadial: false, // optional
        // workerEnabled: false, // optional
    },
    {
        type: 'force',
        preventOverlap: true,
        // center: [200, 200], // optional
        linkDistance: 50, // optional
        nodeStrength: 30, // optional
        edgeStrength: 0.8, // optional
        collideStrength: 0.8, // optional
        nodeSize: 30, // optional
        alpha: 0.9, // optional
        alphaDecay: 0.3, // optional
        alphaMin: 0.01, // optional
        forceSimulation: null, // optional
        onTick: () => {
            // optional
            console.log('ticking');
        },
        onLayoutEnd: () => {
            // optional
            console.log('force layout done');
        },
    },
    {
        type: 'gForce',
        linkDistance: 150, // optional
        nodeStrength: 30, // optional
        edgeStrength: 0.1, // optional
        nodeSize: 30, // optional
        onTick: () => {
            // optional
            console.log('ticking');
        },
        onLayoutEnd: () => {
            // optional
            console.log('force layout done');
        },
        workerEnabled: false, // optional
        gpuEnabled: false, // optional
    },
    {
        type: 'concentric',
        maxLevelDiff: 0.5,
        sortBy: 'degree',
        // center: [200, 200], // optional

        // linkDistance: 50, // optional
        // preventOverlap: true, // optional
        // nodeSize: 30, // optional
        // sweep: 10, // optional
        // equidistant: false, // optional
        // startAngle: 0, // optional
        // clockwise: false, // optional
        // maxLevelDiff: 10, // optional
        // sortBy: 'degree', // optional
        // workerEnabled: false, // optional
    },
    {
        type: 'dagre',
        rankdir: 'LR', // optional
        // align: 'DL', // optional
        // nodesep: 20, // optional
        // ranksep: 50, // optional
        // controlPoints: true, // optional
    },
    {
        type: 'fruchterman',
        // center: [200, 200], // optional
        // gravity: 20, // optional
        // speed: 2, // optional
        // clustering: true, // optional
        // clusterGravity: 30, // optional
        // maxIteration: 2000, // optional
        // workerEnabled: false, // optional
        // gpuEnabled: false, // optional
    },
    {
        type: 'mds',
        workerEnabled: false, // optional
    },
    {
        type: 'comboForce',
        // // center: [200, 200], // optional
        // linkDistance: 50, // optional
        // nodeStrength: 30, // optional
        // edgeStrength: 0.1, // optional
        // onTick: () => {
        //   // optional
        //   console.log('ticking');
        // },
        // onLayoutEnd: () => {
        //   // optional
        //   console.log('combo force layout done');
        // },
    },
  ];

  const [type, setLayout] = React.useState('graphin-force');
  const handleChange = value => {
    console.log('value', value);
    setLayout(value);
  };

  const layout = layouts.find(item => item.type === type);

  // fetchData component - start
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
  // fetchData component - end

  useEffect(() => {
    fetchData();
  }, [fetchData]);

  

  // Menu Component
  const { Menu } = ContextMenu;
  const menuOptions = [
        {
            key: 'tag',
            icon: <TagFilled />,
            name: 'Tag (social)',
        },
        {
            key: 'delete',
            icon: <DeleteFilled />,
            name: 'Delete',
        },
        {
            key: 'expand',
            icon: <ExpandAltOutlined />,
            name: 'Expand (Routeplanner)',
        },
        {
          key: 'editnode',
          icon: <EditOutlined />,
          name: 'Edit node',
      },
  ];

  const searchChange = e => {
    this.setState({ searchField: e.target.value });
  }
  
  const menuHandleChange = (menuItem, menuData, graphData) => {
      console.log("menuHandleChange -> menuItem: ", menuData);
      console.log("menuHandleChange -> menuData: ", menuData);
      console.log("menuHandleChange -> graphData: ", graphData);
      //message.info(`Element：${menuData.id}，Action：${menuItem.name}, Key：${menuItem.key}`);
      message.info(`GraphData：${graphData}`);
      let query;
      switch(menuItem.key) {
        case "tag":
            query = '/_admin/aardvark/g6graph/social';
            queryMethod = "GET";
            message.info(`query: ${query}; queryMethod: ${queryMethod}`);
            setQueryMethod(queryMethod);
            setQueryString(query);
            break;
        case "delete":
            deleteNodeModalRef.current.showDeleteNodeModalFromParent(menuData);
            //showDeleteNodeModal(menuData, graphData);
            //showDeleteNodeModal.current();
            /*
            query = `/_api/gharial/routeplanner/vertex/${menuData.id}`;
            queryMethod = "DELETE";
            message.info(`query: ${query}; queryMethod: ${queryMethod}`);
            setQueryMethod(queryMethod);
            setQueryString(query);
            */
            break;
        case "expand":
            query = `/_admin/aardvark/graph/routeplanner?depth=2&limit=250&nodeColor=#2ecc71&nodeColorAttribute=&nodeColorByCollection=true&edgeColor=#cccccc&edgeColorAttribute=&edgeColorByCollection=false&nodeLabel=_key&edgeLabel=&nodeSize=&nodeSizeByEdges=true&edgeEditable=true&nodeLabelByCollection=false&edgeLabelByCollection=false&nodeStart=&barnesHutOptimize=true&query=FOR v, e, p IN 1..1 ANY "${menuData.id}" GRAPH "routeplanner" RETURN p`;
            queryMethod = "GET";
            message.info(`query: ${query}; queryMethod: ${queryMethod}`);
            setQueryMethod(queryMethod);
            setQueryString(query);
            break;
        case "editnode":
            //query = `/_admin/aardvark/graph/routeplanner?depth=2&limit=250&nodeColor=#2ecc71&nodeColorAttribute=&nodeColorByCollection=true&edgeColor=#cccccc&edgeColorAttribute=&edgeColorByCollection=false&nodeLabel=_key&edgeLabel=&nodeSize=&nodeSizeByEdges=true&edgeEditable=true&nodeLabelByCollection=false&edgeLabelByCollection=false&nodeStart=&barnesHutOptimize=true&query=FOR v, e, p IN 1..1 ANY "${menuData.id}" GRAPH "routeplanner" RETURN p`;
            //queryMethod = "GET";
            message.info(`Open Modal with text editor`);
            toggleEditNodeModal(true);
            showEditNodeModal(graphData);
            break;
        default:
            break;
      }
      /*
      const {loading, data, error} = useFetch(arangoHelper.databaseUrl(query));
      if(loading) {
        message.info(`Deleting node`);
      }
      if(error) {
        message.info(`Error deleting node: ${JSON.stringify(error, null, 2)}`);
      }
      if(data) {
        console.log("DATA after deleting node:");
        console.log(data);
      }
      */
      message.info("AFTER deleting");
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
  // /_admin/aardvark/g6graph/social
  // /_admin/aardvark/g6graph/routeplanner
  if(graphData) {
    console.log("graphdata (nodes) in menu graph");
    console.log(graphData.nodes);
    console.log("graphdata (edges) in menu graph");
    console.log(graphData.edges);
    return (
      <div>
        <Card
          title="G6 Graph Viewer"
          extra={
            <>
              <button onClick={() => graphHelpModalRef.current.showG6GraphHelpFromParent('somedata')}>Open GraphHelpModal (from parent)</button>
              <G6GraphHelpModal ref={graphHelpModalRef} />
              <DeleteNodeModal node={currentNode} show={deleteNodeModalShow} setShow={setDeleteNodeModalShow} />
              <button onClick={masterFunc}>Change data for DeleteNodeModal</button>
              <button onClick={toggleDeleteNodeModal}>Toggle DeleteNodeModal</button>
              <AqlEditor
                queryString={queryString}
                onNewSearch={(myString) => {setQueryString(myString)}}
                onQueryChange={(myString) => {setQueryString(myString)}}
                onOnclickHandler={(myString) => {setQueryString(`/_admin/aardvark/g6graph/${myString}`)}} />
              <NodeList
                nodes={graphData.nodes}
                graphData={graphData}
                onNodeInfo={() => console.log('onNodeInfo() in MenuGraph')}
                onDeleteNode={onDeleteNode} />
              <EdgeList edges={graphData.edges} />
              <LayoutSelector options={layouts} value={type} onChange={handleChange} />
              <GraphDataInfo graphData={graphData}/>
            </>
          }
        >
          <Graphin data={graphData} layout={layout}>
            <ContextMenu>
              <Menu options={menuOptions} onChange={menuHandleChange} graphData={graphData} bindType="node" />
            </ContextMenu>
            <MiniMap />
          </Graphin>
        </Card>
      </div>
    );
  } else {
    return null;
  }

}

export default MenuGraph;
