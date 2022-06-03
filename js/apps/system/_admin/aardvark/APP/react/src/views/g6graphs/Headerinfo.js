import React, { useCallback, useEffect, useState } from 'react';
import { Dropdown, Switch, Input, Space, Menu, Tag, Card, Select, PageHeader, Tabs, Button, Statistic, Descriptions, Tooltip } from 'antd';
import GraphDataInfo from './GraphDataInfo';
import { RollbackOutlined, InfoCircleOutlined, SaveOutlined, NodeIndexOutlined, NodeExpandOutlined, DownloadOutlined, FullscreenOutlined, ShareAltOutlined, CameraOutlined, SearchOutlined } from '@ant-design/icons';
import LayoutSelector from './LayoutSelector.js';
import { ResponseInfo } from './ResponseInfo';
import { data2 } from './data2';
import { NodeList } from './components/node-list/node-list.component';
import { EdgeList } from './components/edge-list/edge-list.component';
import ParameterNodeStart from "./ParameterNodeStart";
import ParameterDepth from "./ParameterDepth";
import ParameterLimit from "./ParameterLimit";
import ParameterNodeLabelByCollection from "./ParameterNodeLabelByCollection";
import ParameterNodeColorByCollection from "./ParameterNodeColorByCollection";
import ParameterEdgeLabelByCollection from "./ParameterEdgeLabelByCollection";
import ParameterEdgeColorByCollection from "./ParameterEdgeColorByCollection";
import ParameterNodeLabel from "./ParameterNodeLabel";
import ParameterEdgeLabel from "./ParameterEdgeLabel";
import ParameterNodeColorAttribute from "./ParameterNodeColorAttribute";
import ParameterEdgeColorAttribute from "./ParameterEdgeColorAttribute";
import ParameterNodeColor from "./ParameterNodeColor";
import ParameterEdgeColor from "./ParameterEdgeColor";
import ParameterNodeSize from './ParameterNodeSize';
import ParameterNodeSizeByEdges from "./ParameterNodeSizeByEdges";
import ButtonSave from "./ButtonSave";
import ButtonRestoreDefault from "./ButtonRestoreDefault";
import EdgeStyleSelector from "./EdgeStyleSelector";
import GraphLayoutSelector from "./GraphLayoutSelector";
import SearchNodes from "./SearchNodes";
import SearchEdges from "./SearchEdges";
import LoadingSpinner from './LoadingSpinner.js';

export const Headerinfo = ({ graphName, graphData, responseDuration, nodesColorAttributes, onDownloadScreenshot, onDownloadFullScreenshot, onChangeLayout, onChangeGraphData, onLoadFullGraph, onDocumentSelect, onNodeSearched, onEdgeSearched, onEdgeStyleChanged, onGraphLayoutChange, onGraphDataLoaded, onIsLoadingData }) => {
  
  const [layout, setLayout] = useState('gForce');
  const [isLoadingData, setIsLoadingData] = useState(false);
  const { Option } = Select;
  const { TabPane } = Tabs;

  function handleChange(value) {
    console.log(`selected ${value}`);
  }

  function onSwitchChange(checked) {
    console.log(`switch to ${checked}`);
  }

  const changeLayout = ({ newLayout }) => {
    /*
    this.graph.updateLayout({
      type: value,
    });
    */
   console.log("New graph newLayout: ", newLayout);
  }

  const renderContent = (column = 2) => (
    <>
      <Descriptions size="small" column={column}>
        <Descriptions.Item label="Response time">
          <ResponseInfo
            duration={responseDuration}
          />ms
        </Descriptions.Item>
      </Descriptions>
      <Tag color="cyan">{graphData.nodes.length} nodes</Tag>
      <Tag color="magenta">{graphData.edges.length} edges</Tag>
    </>
  );

  const Content = ({ children, extra }) => (
    <div className="content">
      <div className="main">{children}</div>
      <div className="extra">{extra}</div>
    </div>
  );

  const menuActionButtons = <>
    <Space>
      <ButtonSave
        graphName={graphName}
        onGraphDataLoaded={(newGraphData) => onGraphDataLoaded(newGraphData)}
        onIsLoadingData={(isLoadingData) => setIsLoadingData(isLoadingData)}
      />
    </Space>
  </>;

const screenshotMenu = (
  <Menu>
    <Menu.Item onClick={() => {
      onDownloadScreenshot();
    }}>
      Download visible graph
    </Menu.Item>
    <Menu.Item onClick={() => {
      onDownloadFullScreenshot();
    }}>
      Download full graph
    </Menu.Item>
  </Menu>
);

  return (
    <>
      <PageHeader
        id="headerinfo"
        className="site-page-header-responsive"
        onBack={() => window.history.back()}
        title={graphName}
        subTitle=""
        extra={[
          <>
            <Tooltip placement="bottom" title={"Fetch full graph - use with caution"}>
              <Button key="1" onClick={onLoadFullGraph()}><DownloadOutlined /></Button>
            </Tooltip>
            
            <Dropdown overlay={screenshotMenu} placement="bottomRight">
              <Button key="2"><CameraOutlined /></Button>
            </Dropdown>

            <Tooltip placement="bottom" title={"Switch to fullscreen mode"}>
              <Button key="3"
                onClick={() => {
                  const elem = document.getElementById("graph-card");
                  if (elem.requestFullscreen) {
                    elem.requestFullscreen();
                  }}}><FullscreenOutlined />
              </Button>
            </Tooltip>

            <Tooltip placement="bottom" title={"Use old graph viewer"}>
              <Button key="4"
                onClick={() => {
                  window.location.href = `/_db/_system/_admin/aardvark/index.html#graph/${graphName}`;
                }}><RollbackOutlined />
              </Button>
            </Tooltip>
          </>
        ]}
        ghost={false}
        footer={
          <Tabs defaultActiveKey="1" type="card" size="small" tabBarExtraContent={menuActionButtons}>
            <TabPane
              tab={
                <span style={{ 'color': '#2ecc71' }}>
                  <ShareAltOutlined />
                  Graph
                </span>
              }
              key="1"
            >
              <ParameterNodeStart
                nodes={graphData.nodes}
                onNodeSelect={(node) => onDocumentSelect(node)}
              />
              <br />
              <GraphLayoutSelector
                onGraphLayoutChange={(layout) => {
                  console.log("onGraphLayoutChange (layout): ", layout);
                  onGraphLayoutChange(layout);
                }}
              />
              <ParameterDepth />
              <ParameterLimit />
            </TabPane>
            <TabPane
              tab={
                <span style={{ 'color': '#2ecc71' }}>
                  <NodeExpandOutlined />
                  Nodes
                </span>
              }
              key="2"
            >
              <ParameterNodeLabel />
              <br />
              <ParameterNodeLabelByCollection
                graphData={graphData}
                onAddCollectionNameChange={
                  (nodeLabelByCollection) => console.log("nodeLabelByCollection: ", nodeLabelByCollection)
                } />
              <br />
              <ParameterNodeColorByCollection />
              <br />
              <ParameterNodeColor />
              <br />
              <ParameterNodeColorAttribute nodesColorAttributes={nodesColorAttributes} />
              <br />
              <ParameterNodeSizeByEdges />
              <br />
              <ParameterNodeSize />
            </TabPane>
            <TabPane
              tab={
                <span style={{ 'color': '#2ecc71' }}>
                  <NodeIndexOutlined />
                  Edges
                </span>
              }
              key="3"
            >
              <ParameterEdgeLabel />
              <br />
              <ParameterEdgeLabelByCollection />
              <br />
              <ParameterEdgeColorByCollection />
              <br />
              <ParameterEdgeColor />
              <br />
              <ParameterEdgeColorAttribute />
              <br />
              <EdgeStyleSelector
                onEdgeStyleChange={(typeModel) => {
                  console.log("onEdgeStyleChange (typeModel): ", typeModel);
                  onEdgeStyleChanged(typeModel);
                }}
              />
            </TabPane>
            <TabPane
              tab={
                <span style={{ 'color': '#2ecc71' }}>
                  <SearchOutlined />
                  Search
                </span>
              }
              key="4"
            >
              <SearchNodes
                nodes={graphData.nodes}
                graphData={graphData}
                onNodeInfo={() => console.log('onNodeInfo() in MenuGraph')}
                onNodeSelect={(previousSearchedNode, node) => onNodeSearched(previousSearchedNode, node)}
              />
              <SearchEdges
                edges={graphData.edges}
                graphData={graphData}
                onEdgeInfo={() => console.log('onEdgeInfo() in MenuGraph')}
                onEdgeSelect={(previousSearchedEdge, edge) => onEdgeSearched(previousSearchedEdge, edge)}
              />
            </TabPane>
          </Tabs>
        }
      >
        <Content>{renderContent()}</Content>
      </PageHeader>
      {isLoadingData ? <LoadingSpinner /> : null}
    </>
  );
}
