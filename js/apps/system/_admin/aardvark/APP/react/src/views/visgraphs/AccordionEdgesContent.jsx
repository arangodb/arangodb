import React from "react";
import { Stack } from "@chakra-ui/react";
import ParameterEdgeLabel from "./ParameterEdgeLabel";
import ParameterEdgeColor from "./ParameterEdgeColor";
import ParameterEdgeLabelByCollection from "./ParameterEdgeLabelByCollection";
import ParameterEdgeColorByCollection from "./ParameterEdgeColorByCollection";
import ParameterEdgeColorAttribute from "./ParameterEdgeColorAttribute";
import ParameterEdgeDirection from "./ParameterEdgeDirection";
import EdgeStyleSelector from "./EdgeStyleSelector";

export const AccordionEdgesContent = () => {
  return (
    <Stack>
      <ParameterEdgeLabel />
      <ParameterEdgeColor />
      <ParameterEdgeLabelByCollection />
      <ParameterEdgeColorByCollection />
      <ParameterEdgeColorAttribute />
      <ParameterEdgeDirection />
      <EdgeStyleSelector />
    </Stack>
  );
};
