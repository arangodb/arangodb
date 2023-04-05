import { Icon, IconButton, Tooltip } from "@chakra-ui/react";
import React from "react";
import { AddAPhoto } from "styled-icons/material";
import { useGraph } from "../GraphContext";

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
export const DownloadGraphButton = () => {
  const { graphName } = useGraph();
  return (
    <Tooltip hasArrow label={"Take a screenshot"} placement="bottom">
      <IconButton
        onClick={() => downloadCanvas(graphName)}
        size="sm"
        icon={<Icon width="5" height="5" as={AddAPhoto} />}
        aria-label={"Take a screenshot"} />
    </Tooltip>
  );
};
