import { Box, HStack } from "@chakra-ui/react";
import React from "react";
import { useGraph } from "../GraphContext";
import { DownloadGraphButton } from "./DownloadGraphButton";
import { FullscreenGraphButton } from "./FullscreenGraphButton";
import { GraphSettings } from "../graphSettings/GraphSettings";
import { LoadFullGraphButton } from "./LoadFullGraphButton";
import { SearchGraphButton } from "./SearchGraphButton";
import { SwitchToOldButton } from "./SwitchToOldButton";

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
      <RightActions />
    </Box>
  );
};
const RightActions = () => {
  return (
    <HStack justifyContent="end" alignItems="center">
      <DownloadGraphButton />
      <FullscreenGraphButton />
      <LoadFullGraphButton />
      <SwitchToOldButton />
      <SearchGraphButton />
      <GraphSettings />
    </HStack>
  );
};
