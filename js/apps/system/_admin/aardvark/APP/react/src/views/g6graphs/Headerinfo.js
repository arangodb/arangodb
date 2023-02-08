import React, { useState } from 'react';
import { Dropdown, Space, Menu, Tag, PageHeader, Button, Descriptions, Tooltip } from 'antd';
import { RollbackOutlined, NodeIndexOutlined, NodeExpandOutlined, DownloadOutlined, FullscreenOutlined, ShareAltOutlined, CameraOutlined, SearchOutlined } from '@ant-design/icons';
import ParameterNodeStart from "./ParameterNodeStart";
import ParameterDepth from "./ParameterDepth";
import ParameterLimit from "./ParameterLimit";
import ParameterNodeLabelByCollection from "./ParameterNodeLabelByCollection";
import ParameterNodeColorByCollection from "./ParameterNodeColorByCollection";
import ParameterEdgeLabelByCollection from "./ParameterEdgeLabelByCollection";
import ParameterEdgeColorByCollection from "./ParameterEdgeColorByCollection";
import ParameterNodeLabel from "./ParameterNodeLabel";
import ParameterEdgeLabel from "./ParameterEdgeLabel";
import { HelpModal } from "./HelpModal";
import ParameterNodeColorAttribute from "./ParameterNodeColorAttribute";
import ParameterEdgeColorAttribute from "./ParameterEdgeColorAttribute";
import ParameterNodeColor from "./ParameterNodeColor";
import ParameterEdgeColor from "./ParameterEdgeColor";
import ParameterNodeSize from './ParameterNodeSize';
import ParameterNodeSizeByEdges from "./ParameterNodeSizeByEdges";
import ButtonSave from "./ButtonSave";
import EdgeStyleSelector from "./EdgeStyleSelector";
import GraphLayoutSelector from "./GraphLayoutSelector";
import VisGraphLayoutSelector from "./VisGraphLayoutSelector";
import SearchNodes from "./SearchNodes";
import SearchEdges from "./SearchEdges";
import LoadingSpinner from './LoadingSpinner.js';
import AccordionView from './components/Accordion/Accordion';

export const Headerinfo = ({ graphName, graphData, responseDuration, nodesColorAttributes, edgesColorAttributes, onDownloadScreenshot, onDownloadFullScreenshot, onChangeLayout, onChangeGraphData, onLoadFullGraph, onDocumentSelect, onNodeSearched, onEdgeSearched, onEdgeStyleChanged, onGraphLayoutChange, onVisGraphLayoutChange, onGraphDataLoaded, onIsLoadingData }) => {
  
  const [isLoadingData, setIsLoadingData] = useState(false);
  const [showHelpModal, setShowHelpModal] = useState(false);

  const enterFullscreen = (element) => {
    if(element.requestFullscreen) {
      element.requestFullscreen();
    } else if(element.msRequestFullscreen) {
      element.msRequestFullscreen();
    } else if(element.webkitRequestFullscreen) {
      element.webkitRequestFullscreen();
    }
  }

  const renderContent = (column = 2) => (
    <>
      <Descriptions size="small" column={column}>
        <Descriptions.Item label="Response time">
          <span>{responseDuration}</span>ms
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
        onGraphDataLoaded={(newGraphData, responseTimesObject) => {
          onGraphDataLoaded(newGraphData, responseTimesObject)}
        }
        onIsLoadingData={(isLoadingData) => setIsLoadingData(isLoadingData)}
      />
    </Space>
  </>;

const screenshotMenu = (
  <Menu className='graphReactViewContainer'>
    <Tooltip placement="left" title={"Screenshot of the visible graph"} overlayClassName='graph-border-box' >
      <Menu.Item onClick={() => {
        onDownloadScreenshot();
      }} className='graphReactViewContainer'>
        Download visible graph
      </Menu.Item>
    </Tooltip>
    <Tooltip placement="left" title={"Screenshot of the full graph"} overlayClassName='graph-border-box' >
      <Menu.Item onClick={() => {
        onDownloadFullScreenshot();
      }} className='graphReactViewContainer'>
        Download full graph
      </Menu.Item>
    </Tooltip>
  </Menu>
);

  const AccordionGraphContent = () => {
    return (
      <>
        <ParameterNodeStart
          nodes={graphData.nodes}
          onNodeSelect={(node) => onDocumentSelect(node)}
        />
        <br />
        <GraphLayoutSelector
          onGraphLayoutChange={(layout) => {
            onGraphLayoutChange(layout);
          }}
        />
        <VisGraphLayoutSelector
          onVisGraphLayoutChange={(layout) => {
            console.log("VisGraphLayoutSelector recieved the layout: ", layout);
            onVisGraphLayoutChange(layout);
          }}
        />
        <ParameterDepth />
        <ParameterLimit />
      </>)
  };

  const AccordionNodesContent = () => {
    return (
      <>
        <ParameterNodeLabel />
        <br />
        <ParameterNodeColor />
        <br />
        <ParameterNodeLabelByCollection graphData={graphData} />
        <br />
        <ParameterNodeColorByCollection />
        <br />
        <ParameterNodeColorAttribute nodesColorAttributes={nodesColorAttributes} />
        <br />
        <ParameterNodeSizeByEdges />
        <br />
        <ParameterNodeSize />
      </>)
  };

  const AccordionEdgesContent = () => {
    return (
      <>
        <ParameterEdgeLabel />
        <br />
        <ParameterEdgeColor />
        <br />
        <ParameterEdgeLabelByCollection />
        <br />
        <ParameterEdgeColorByCollection />
        <br />
        <ParameterEdgeColorAttribute edgesColorAttributes={edgesColorAttributes}/>
        <br />
        <EdgeStyleSelector
          onEdgeStyleChange={(typeModel) => {
            onEdgeStyleChanged(typeModel);
          }}
        />
      </>)
  };

  return (
    <>
      <div style={{ 'background': '#404a53' }}>
        <div style={{ 'padding': '24px' }}>
          <ButtonSave
            graphName={graphName}
            onGraphDataLoaded={(newGraphData, responseTimesObject) => {
              onGraphDataLoaded(newGraphData, responseTimesObject)}
            }
            onIsLoadingData={(isLoadingData) => setIsLoadingData(isLoadingData)}
          />
        </div>
        <AccordionView
          allowMultipleOpen
          accordionConfig={[
            {
              index: 0,
              content: <div><AccordionGraphContent /></div>,
              label: "Graph",
              testID: "accordionItem0",
              defaultActive: true
            },
            {
              index: 1,
              content: (
                <div><AccordionNodesContent /></div>
              ),
              label: "Nodes",
              testID: "accordionItem1"
            },
            {
              index: 2,
              content: (
                <div><AccordionEdgesContent /></div>
              ),
              label: "Edges",
              testID: "accordionItem2"
            }
          ]}
        />
      </div>
      <HelpModal
        shouldShow={showHelpModal}
        onRequestClose={() => {
          setShowHelpModal(false);
        }}
      >
      </HelpModal>
      <PageHeader
        id="headerinfo"
        className="site-page-header-responsive"
        onBack={() => window.history.back()}
        title={graphName}
        subTitle=""
        extra={[
          <>
            <Dropdown overlayClassName='graphReactViewContainer screenshot-button' overlay={screenshotMenu} placement="bottomRight">
              <Button key="2"><DownloadOutlined /></Button>
            </Dropdown>

            <Tooltip placement="left" title={"Enter full screen"} overlayClassName='graph-border-box' >
              <Button key="3"
                onClick={() => {
                  const elem = document.getElementById("graph-card");
                  enterFullscreen(elem);
                  }}>
                    <FullscreenOutlined />
              </Button>
            </Tooltip>

            <Tooltip placement="left" title={"Switch to the old graph viewer"} overlayClassName='graph-border-box' >
              <Button key="4"
                onClick={() => {
                  window.location.href = `/_db/_system/_admin/aardvark/index.html#graph/${graphName}`;
                }}><RollbackOutlined />
              </Button>
            </Tooltip>

            <Tooltip placement="left" title={"Get instructions and support on how to use the graph viewer"} overlayClassName='graph-border-box' >
              <Button key="5"
                onClick={() => {
                  setShowHelpModal(true);
                }}><i class="fa fa-question-circle" style={{ 'fontSize': '18px', 'marginTop': '1px' }}></i>
              </Button>
            </Tooltip>
          </>
        ]}
        ghost={false}
      >
        <Content>{renderContent()}</Content>
      </PageHeader>
      {isLoadingData ? <LoadingSpinner /> : null}
    </>
  );
}
