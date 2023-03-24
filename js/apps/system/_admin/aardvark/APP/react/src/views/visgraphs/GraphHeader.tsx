import { DownloadIcon } from "@chakra-ui/icons";
import { Box, HStack, Icon, IconButton } from "@chakra-ui/react";
import React from "react";
import { CloudDownload, Fullscreen, Undo } from "styled-icons/material";
import { useGraph } from "./GraphContext";
import { GraphSettings } from "./GraphSettings";

export const GraphHeader = () => {
  const { graphName } = useGraph();

  return (
    <Box
      boxShadow="md"
      height="10"
      paddingX="2"
      display="grid"
      alignItems="center"
      gridTemplateColumns="1fr 1fr"
    >
      <Box>{graphName}</Box>
      <HStack justifyContent="end" alignItems="center">
        <DownloadButton />
        <FullscreenButton />
        <LoadFullGraph />
        <SwitchToOld />
        <GraphSettings />
      </HStack>
    </Box>
  );
};
const downloadCanvas = (graphName: string) => {
  let canvas = document.getElementsByTagName("canvas")[0];

  // set canvas background to white for screenshot download
  let context = canvas.getContext("2d");
  if (!context) {
    return;
  }
  context.globalCompositeOperation = "destination-over";
  context.fillStyle = "#ffffff";
  context.fillRect(0, 0, canvas.width, canvas.height);

  let canvasUrl = canvas.toDataURL("image/jpeg", 1);
  const createEl = document.createElement("a");
  createEl.href = canvasUrl;
  createEl.download = `${graphName}`;
  createEl.click();
  createEl.remove();
};
const DownloadButton = () => {
  const { graphName } = useGraph();
  return (
    <IconButton
      onClick={() => downloadCanvas(graphName)}
      size="sm"
      icon={<DownloadIcon />}
      aria-label={"Download screenshot"}
    />
  );
};

const enterFullscreen = (element: any) => {
  if (element.requestFullscreen) {
    element.requestFullscreen();
  } else if (element.webkitRequestFullscreen) {
    /* Safari */
    element.webkitRequestFullscreen();
  } else if (element.msRequestFullscreen) {
    /* IE11 */
    element.msRequestFullscreen();
  }
};

const FullscreenButton = () => {
  return (
    <IconButton
      size="sm"
      onClick={() => {
        const elem = document.getElementById("graphNetworkDiv");
        enterFullscreen(elem);
      }}
      icon={<Icon width="5" height="5" as={Fullscreen} />}
      aria-label={"Enter fullscreen"}
    />
  );
};

const SwitchToOld = () => {
  const { graphName } = useGraph();
  return (
    <IconButton
      size="sm"
      onClick={() => {
        window.location.href = `/_db/_system/_admin/aardvark/index.html#graph/${graphName}`;
      }}
      icon={<Icon width="5" height="5" as={Undo} />}
      aria-label={"Enter fullscreen"}
    />
  );
};

const LoadFullGraph = () => {
  const { setLoadFullGraph } = useGraph();
  return (
    <IconButton
      size="sm"
      onClick={() => {
        setLoadFullGraph(true);
      }}
      icon={<Icon width="5" height="5" as={CloudDownload} />}
      aria-label={"Load full graph"}
    />
  );
};
