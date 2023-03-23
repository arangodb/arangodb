import React from "react";
import ParameterNodeStart from "./ParameterNodeStart";
import ParameterDepth from "./ParameterDepth";
import ParameterLimit from "./ParameterLimit";
import GraphLayoutSelector from "./GraphLayoutSelector";
import { Stack } from "@chakra-ui/react";

export const AccordionGraphContent = () => {
  return (
    <Stack>
      <ParameterNodeStart />
      <GraphLayoutSelector />
      <ParameterDepth />
      <ParameterLimit />
    </Stack>
  );
};
