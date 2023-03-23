import React from "react";
import { Stack } from "@chakra-ui/react";
import ParameterNodeLabel from "./ParameterNodeLabel";
import ParameterNodeColor from "./ParameterNodeColor";
import ParameterNodeLabelByCollection from "./ParameterNodeLabelByCollection";
import ParameterNodeColorByCollection from "./ParameterNodeColorByCollection";
import ParameterNodeColorAttribute from "./ParameterNodeColorAttribute";
import ParameterNodeSizeByEdges from "./ParameterNodeSizeByEdges";
import ParameterNodeSize from "./ParameterNodeSize";

export const AccordionNodesContent = () => {
  return (
    <Stack>
      <ParameterNodeLabel />
      <ParameterNodeColor />
      <ParameterNodeLabelByCollection />
      <ParameterNodeColorByCollection />
      <ParameterNodeColorAttribute />
      <ParameterNodeSizeByEdges />
      <ParameterNodeSize />
    </Stack>
  );
};
