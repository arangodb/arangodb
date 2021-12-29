/* global arangoHelper, arangoFetch, frontendConfig */

import React, { useEffect, useState } from 'react';
import ReactDOM from 'react-dom';
import Graphin, { Utils, Behaviors, GraphinContext } from '@antv/graphin';
import { message, Select, Row, Col, Card } from 'antd';
import { MiniMap } from '@antv/graphin-components';
import { ContextMenu, FishEye } from '@antv/graphin-components';
import SocialGraph from './SocialGraph.js';
import AqlEditor from './AqlEditor.js';
import MenuGraph from './MenuGraph.js';
import HorizonGraph from './HorizonGraph.js';
import LayoutedGraph from './LayoutedGraph.js';
import DonutGraph from './DonutGraph.js';
import DataGraph from './DataGraph.js';
import 'antd/dist/antd.css';
import iconLoader from '@antv/graphin-icons';
import '@antv/graphin-icons/dist/index.css';
import G6JsGraph from './G6JsGraph';

import "@antv/graphin/dist/index.css";
//import "@antv/graphin-components/dist/index.css";
//import "./styles.css";
import "@antv/graphin/dist/index.css"; // import css fro graphin
import "@antv/graphin-components/dist/index.css"; // Graphin Component CSS
//import './styles.css';

import {
    TrademarkCircleFilled,
    ChromeFilled,
    BranchesOutlined,
    ApartmentOutlined,
    AppstoreFilled,
    CopyrightCircleFilled,
    CustomerServiceFilled,
    ShareAltOutlined,
    TagFilled,
    DeleteFilled,
    ExpandAltOutlined,

} from '@ant-design/icons';

// Register in Graphin
const { fontFamily, glyphs } = iconLoader();
const icons = Graphin.registerFontFamily(iconLoader);
const { Menu, Donut } = ContextMenu;
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

// /_admin/aardvark/graph/routeplanner?depth=2&limit=250&nodeColor=#2ecc71&nodeColorAttribute=&nodeColorByCollection=true&edgeColor=#cccccc&edgeColorAttribute=&edgeColorByCollection=false&nodeLabel=_key&edgeLabel=&nodeSize=&nodeSizeByEdges=true&edgeEditable=true&nodeLabelByCollection=false&edgeLabelByCollection=false&nodeStart=&barnesHutOptimize=true&query=FOR v, e, p IN 1..1 ANY "male/bob" GRAPH "routeplanner" RETURN p

function G6Graph() {
  const [state, setState] = React.useState({
    data2: null,
  });
  React.useEffect(() => {
    // eslint-disable-next-line no-undef
    fetch('https://gw.alipayobjects.com/os/antvdemo/assets/data/algorithm-category.json')
    .then(res => res.json())
    .then(res => {
      console.log('data', res);
      setState({
        data2: res,
      });
    });
  }, []);
  const { data2 } = state;

  return (
    <div>
          <Card
            title="G6Graph"
          >
              {data2 && (
                <Graphin
                  data={data2}
                >
                </Graphin>
              )}
          </Card>
    </div>
  );
}

const G6GraphReactView = () => {
    return <div>
        <DataGraph />
    </div>;
};

window.G6GraphReactView = G6GraphReactView;
