/* global arangoHelper, arangoFetch, frontendConfig */

import React, { useEffect, useState, useCallback } from 'react';
import Graphin, { Utils, GraphinContext } from '@antv/graphin';
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

const MenuGraph = () => {
  let [queryString, setQueryString] = useState("/_admin/aardvark/g6graph/routeplanner");
  let [graphData, setGraphData] = useState(null);
  let [queryMethod, setQueryMethod] = useState("GET");

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

  const SelectOption = Select.Option;
  const LayoutSelector = props => {
    const { value, onChange, options } = props;

    return (
      <div>
        <Select style={{ width: '120px' }} value={value} onChange={onChange}>
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
  const options = [
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
  
  const HandleChange = (menuItem, menuData) => {
      console.log(menuItem, menuData);
      message.info(`Element：${menuData.id}，Action：${menuItem.name}, Key：${menuItem.key}`);
      let query;
      switch(menuItem.key) {
        case "tag":
            query = '/_admin/aardvark/g6graph/social';
            queryMethod = "GET";
            break;
        case "delete":
            query = `/_api/gharial/routeplanner/vertex/${menuData.id}`;
            queryMethod = "DELETE";
            break;
        case "expand":
            query = `/_admin/aardvark/graph/routeplanner?depth=2&limit=250&nodeColor=#2ecc71&nodeColorAttribute=&nodeColorByCollection=true&edgeColor=#cccccc&edgeColorAttribute=&edgeColorByCollection=false&nodeLabel=_key&edgeLabel=&nodeSize=&nodeSizeByEdges=true&edgeEditable=true&nodeLabelByCollection=false&edgeLabelByCollection=false&nodeStart=&barnesHutOptimize=true&query=FOR v, e, p IN 1..1 ANY "${menuData.id}" GRAPH "routeplanner" RETURN p`;
            queryMethod = "GET";
            break;
        case "editnode":
            query = `/_admin/aardvark/graph/routeplanner?depth=2&limit=250&nodeColor=#2ecc71&nodeColorAttribute=&nodeColorByCollection=true&edgeColor=#cccccc&edgeColorAttribute=&edgeColorByCollection=false&nodeLabel=_key&edgeLabel=&nodeSize=&nodeSizeByEdges=true&edgeEditable=true&nodeLabelByCollection=false&edgeLabelByCollection=false&nodeStart=&barnesHutOptimize=true&query=FOR v, e, p IN 1..1 ANY "${menuData.id}" GRAPH "routeplanner" RETURN p`;
            queryMethod = "GET";
            break;
        default:
            break;
      }
      message.info(`query: ${query}; queryMethod: ${queryMethod}`);
      setQueryMethod(queryMethod);
      setQueryString(query);
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

  // /_admin/aardvark/g6graph/social
  // /_admin/aardvark/g6graph/routeplanner
  if(graphData) {
    console.log("graphdata (nodes) in menu graph");
    console.log(graphData.nodes);
    console.log("graphdata (edges) in menu graph");
    console.log(graphData.edges);
    return (
      <div>
        <NodeList
          nodes={graphData.nodes}
          onNodeInfo={() => console.log('onNodeInfo() in MenuGraph')} />
        <EdgeList edges={graphData.edges} />
        <AqlEditor
          queryString={queryString}
          onNewSearch={(myString) => {setQueryString(myString)}}
          onQueryChange={(myString) => {setQueryString(myString)}}
          onOnclickHandler={(myString) => {setQueryString(myString)}} />
        <Card
          title="G6 Graph Viewer"
          extra={<LayoutSelector options={layouts} value={type} onChange={handleChange} />}
        >
          <Graphin data={graphData} layout={layout}>
            <ContextMenu>
              <Menu options={options} onChange={HandleChange} bindType="node" />
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
