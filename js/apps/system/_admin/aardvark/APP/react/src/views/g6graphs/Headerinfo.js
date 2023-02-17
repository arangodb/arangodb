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

  const enterFullscreen = (elem) => {
    if (elem.requestFullscreen) {
      elem.requestFullscreen();
    } else if (elem.webkitRequestFullscreen) { /* Safari */
      elem.webkitRequestFullscreen();
    } else if (elem.msRequestFullscreen) { /* IE11 */
      elem.msRequestFullscreen();
    }
  }

  const AccordionGraphContent = () => {
    return (
      <>
        <ParameterNodeStart
          nodes={graphData.nodes}
          onNodeSelect={(node) => onDocumentSelect(node)}
        />
        <br />
        <GraphLayoutSelector />
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
      <div
        class="graphViewerNavbar"
        style={{ 'width': '100%', 'height': '40px', 'background': '#ffffff', 'display': 'flex', 'alignItems': 'center', 'padding': '8px', 'marginBottom': '24px', 'borderBottom': '2px solid #d9dbdc' }}
      >
        {graphName}

        <div style={{ 'marginLeft': 'auto' }}>
          <ToolTip
            title={"Download screenshot"}
            setArrow={true}
          >
            <button
              onClick={() => {
                let canvas = document.getElementsByTagName('canvas')[0];
                
                // set canvas nackground to white for screenshot download
                let context = canvas.getContext("2d");
                context.globalCompositeOperation = "destination-over";
                context.fillStyle = '#ffffff';
                context.fillRect(0, 0, canvas.width, canvas.height);
                
                let canvasUrl = canvas.toDataURL("image/jpeg", 1);
                const createEl = document.createElement('a');
                createEl.style.backgroundColor = '#ffffff';
                createEl.href = canvasUrl;
                createEl.download = `${graphName}`;
                createEl.click();
                createEl.remove();
              }}
              style={{
                'background': '#fff',
                'border': 0,
                'marginLeft': 'auto'
              }}><i class="fa fa-download" style={{ 'fontSize': '18px', 'marginTop': '6px', 'color': '#555' }}></i>
            </button>
          </ToolTip>

          <ToolTip
            title={"Enter full screen"}
            setArrow={true}
          >
            <button
              onClick={() => {
                const elem = document.getElementById("visnetworkdiv");
                enterFullscreen(elem);
              }}
              style={{
                'background': '#fff',
                'border': 0
              }}><i class="fa fa-arrows-alt" style={{ 'fontSize': '18px', 'marginTop': '6px', 'color': '#555' }}></i>
            </button>
          </ToolTip>

          <ToolTip
            title={"Switch to the old graph viewer"}
            setArrow={true}
          >
            <button
              onClick={() => {
                window.location.href = `/_db/_system/_admin/aardvark/index.html#graph/${graphName}`;
              }}
              style={{
                'background': '#fff',
                'border': 0
              }}><i class="fa fa-retweet" style={{ 'fontSize': '18px', 'marginTop': '6px', 'color': '#555' }}></i>
            </button>
          </ToolTip>

          <ToolTip
            title={"Get instructions and support on how to use the graph viewer"}
            setArrow={true}
          >
            <button
              onClick={() => {
                setShowHelpModal(true);
              }}
              style={{
                'background': '#fff',
                'border': 0
              }}><i class="fa fa-question-circle" style={{ 'fontSize': '18px', 'marginTop': '6px', 'color': '#555' }}></i>
            </button>
          </ToolTip>
        
        <IconButton icon={'bars'} onClick={() => {
          toggleDrawer(!open)
        }}
          style={{
          background: '#2ECC71',
          color: 'white',
          paddingLeft: '14px',
          marginLeft: 'auto'
        }}>
          Settings
        </IconButton>
        </div>
      </div>
      <Drawer
        position="right"
        open={open}
       >
        <div style={{ 'background': '#404a53' }}>
          <div style={{ 'padding': '24px', 'display': 'flex' }}>
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
      <HelpModal
        shouldShow={showHelpModal}
        onRequestClose={() => {
          setShowHelpModal(false);
        }}
      >
      </HelpModal>
      {isLoadingData ? <LoadingSpinner /> : null}
    </>
  );
}
