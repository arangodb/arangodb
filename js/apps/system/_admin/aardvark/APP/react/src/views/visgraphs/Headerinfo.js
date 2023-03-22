import React, { useRef, useState } from 'react';
import ToolTip from '../../components/arango/tootip';
import { IconButton } from "../../components/arango/buttons";
import {
  Box,
  Flex,
  Center} from '@chakra-ui/react';
import { GraphSlideOutPane } from './GraphSlideOutPane';

export const Headerinfo = ({ graphName, responseDuration, onChangeGraphData, onGraphDataLoaded, onIsLoadingData }) => {
  
  const [isLoadingData, setIsLoadingData] = useState(false);

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

  const navbarRef = useRef();

  return (
    <>
      <div
        id="graphViewerNavbarId"
        class="graphViewerNavbar"
        style={{ 'width': '100%', 'height': '40px', 'background': '#ffffff', 'display': 'flex', 'alignItems': 'center', 'padding': '8px', 'marginBottom': '24px', 'borderBottom': '2px solid #d9dbdc' }}
      >
        {graphName}

        <div style={{ 'marginLeft': 'auto' }}>
        <Flex direction='row'>
          <Center>
            <Box mr='3'>
              <ToolTip
                title={"Download screenshot"}
                setArrow={true}
              >
                <button
                  onClick={() => {
                    let canvas = document.getElementsByTagName('canvas')[0];
                    
                    // set canvas background to white for screenshot download
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
            </Box>

            <Box mr='3'>
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
            </Box>

            <Box mr='3'>
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
            </Box>
            
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
          </Center>
        </Flex>
        </div>
      </div>
      <GraphSlideOutPane
        open={open}
        graphName={graphName}
        onGraphDataLoaded={onGraphDataLoaded}
        setIsLoadingData={setIsLoadingData} 
      />
      {isLoadingData ? "Loading..." : null}
    </>
  );
}


