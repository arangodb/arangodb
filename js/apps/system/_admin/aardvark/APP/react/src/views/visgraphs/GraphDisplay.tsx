import { Box, Button, IconButton } from "@chakra-ui/react";
import { DownloadIcon, SettingsIcon } from "@chakra-ui/icons";
import React from "react";
import { GraphContextProvider, useGraph } from "./GraphContext";
import GraphNetwork from "./GraphNetwork";

export const GraphDisplay = () => {
  return (
    <GraphContextProvider>
      <GraphContent />
    </GraphContextProvider>
  );
};

const GraphContent = () => {
  const { graphData } = useGraph();
  if (!graphData) {
    return <>Graph not found</>;
  }
  return (
    <>
      <GraphHeader />
      <GraphNetwork />
    </>
  );
};

const GraphHeader = () => {
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
        <SettingsButton />
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
const SettingsButton = () => {
  return (
    <Button
      size="sm"
      leftIcon={<SettingsIcon />}
      aria-label={"Settings"}
      colorScheme="green"
    >
      Settings
    </Button>
  );
};
