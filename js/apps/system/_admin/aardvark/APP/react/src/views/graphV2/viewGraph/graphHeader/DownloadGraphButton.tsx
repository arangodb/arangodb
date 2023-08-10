import { Icon, IconButton, Tooltip } from "@chakra-ui/react";
import React from "react";
import { AddAPhoto } from "styled-icons/material";
import { useGraph } from "../GraphContext";

export const downloadCanvas = (graphName: string) => {
  const canvas = document.getElementsByTagName("canvas")[0];
  const newCanvas = canvas.cloneNode(true) as HTMLCanvasElement;
  // set canvas background to white for screenshot download
  const context = newCanvas.getContext("2d");
  if (!context) {
    return;
  }
  context.fillStyle = "#ffffff";
  context.fillRect(0, 0, newCanvas.width, newCanvas.height);
  context.drawImage(canvas, 0, 0);
  const canvasUrl = newCanvas.toDataURL("image/jpeg", 1);
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
        aria-label={"Take a screenshot"}
      />
    </Tooltip>
  );
};
