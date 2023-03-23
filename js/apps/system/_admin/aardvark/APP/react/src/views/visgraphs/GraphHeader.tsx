import { DownloadIcon } from "@chakra-ui/icons";
import { Box, IconButton } from "@chakra-ui/react";
import React from "react";
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
      <Box display="flex" justifyContent="end" alignItems="center">
        <DownloadButton />
        <GraphSettings />
      </Box>
    </Box>
  );
};
const DownloadButton = () => {
  return (
    <IconButton
      size="sm"
      icon={<DownloadIcon />}
      aria-label={"Download screenshot"}
    />
  );
};
