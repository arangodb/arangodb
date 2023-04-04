import React from "react";
import { Grid } from "@chakra-ui/react";
import ParameterNodeLabel from "./ParameterNodeLabel";
import ParameterNodeColor from "./ParameterNodeColor";
import ParameterNodeLabelByCollection from "./ParameterNodeLabelByCollection";
import ParameterNodeColorByCollection from "./ParameterNodeColorByCollection";
import ParameterNodeColorAttribute from "./ParameterNodeColorAttribute";
import ParameterNodeSizeByEdges from "./ParameterNodeSizeByEdges";
import ParameterNodeSize from "./ParameterNodeSize";

export const AccordionNodesContent = () => {
  return (
    <Grid
      rowGap="2"
      alignItems="center"
      templateColumns="150px 1fr 40px"
      justifyItems="start"
    >
      <ParameterNodeLabel />
      <ParameterNodeColor />
      <ParameterNodeColorByCollection />
      <ParameterNodeColorAttribute />
      <ParameterNodeLabelByCollection />
      <ParameterNodeSizeByEdges />
      <ParameterNodeSize />
    </Grid>
  );
};
