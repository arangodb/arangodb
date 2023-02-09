import React, { useState } from 'react';
import ToolTip from '../../components/arango/tootip';
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
import Drawer from "./components/Drawer/Drawer";
import { IconButton } from "../../components/arango/buttons";

export const Headerinfo = ({ graphName, graphData, responseDuration, nodesColorAttributes, edgesColorAttributes, onDownloadScreenshot, onDownloadFullScreenshot, onChangeLayout, onChangeGraphData, onLoadFullGraph, onDocumentSelect, onNodeSearched, onEdgeSearched, onEdgeStyleChanged, onGraphLayoutChange, onVisGraphLayoutChange, onGraphDataLoaded, onIsLoadingData }) => {
  
  const [isLoadingData, setIsLoadingData] = useState(false);
  const [showHelpModal, setShowHelpModal] = useState(false);

  const [open, toggleDrawer] = useState(false);

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
      Response time: <span>{responseDuration}</span>ms<br />
      {graphData.nodes.length} nodes (to be Tags)<br />
      {graphData.edges.length} edges (to be Tags)
    </>
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
      <Drawer
        position="right"
        open={open}
        onClose={() => {
          console.log("closed");
        }}
        onOpen={() => {
          console.log("open");
        }}
      >
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
      </Drawer>
      <IconButton icon={'bars'} onClick={() => toggleDrawer(!open)} style={{
          background: '#2ECC71',
          color: 'white',
          paddingLeft: 8,
          paddingTop: 6
        }}>Settings
        </IconButton>
      <HelpModal
        shouldShow={showHelpModal}
        onRequestClose={() => {
          setShowHelpModal(false);
        }}
      >
      </HelpModal>
      <div id="page-header-temp">
        <h2>{graphName}</h2>
        <IconButton icon={'plus-circle'} onClick={() => console.log("clicked")} style={{
          background: 'transparent',
          border: '1px solid #333',
          //color: 'white',
          paddingLeft: 0,
          paddingTop: 0
        }}><i class="fa fa-download" style={{ 'fontSize': '18px', 'marginTop': '1px' }}></i>
        </IconButton>

        <ToolTip
          title={"Enter full screen"}
          setArrow={true}
        >
          <button
            onClick={() => {
              const elem = document.getElementById("graph-card");
              enterFullscreen(elem);
            }}><i class="fa fa-arrows-alt" style={{ 'fontSize': '18px', 'marginTop': '1px' }}></i>
          </button>
        </ToolTip>

        <ToolTip
          title={"Switch to the old graph viewer"}
          setArrow={true}
        >
          <button
            onClick={() => {
              window.location.href = `/_db/_system/_admin/aardvark/index.html#graph/${graphName}`;
            }}><i class="fa fa-retweet" style={{ 'fontSize': '18px', 'marginTop': '1px' }}></i>
          </button>
        </ToolTip>

        <ToolTip
          title={"Get instructions and support on how to use the graph viewer"}
          setArrow={true}
        >
          <button
            onClick={() => {
              setShowHelpModal(true);
            }}><i class="fa fa-question-circle" style={{ 'fontSize': '18px', 'marginTop': '1px' }}></i>
          </button>
        </ToolTip>
        <div id="content">{renderContent()}</div>
      </div>
      {isLoadingData ? <LoadingSpinner /> : null}
    </>
  );
}
