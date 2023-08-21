import { Grid } from "@chakra-ui/react";
import React from "react";
import EdgeStyleSelector from "./EdgeStyleSelector";
import ParameterEdgeColor from "./ParameterEdgeColor";
import ParameterEdgeColorAttribute from "./ParameterEdgeColorAttribute";
import ParameterEdgeColorByCollection from "./ParameterEdgeColorByCollection";
import ParameterEdgeDirection from "./ParameterEdgeDirection";
import ParameterEdgeLabel from "./ParameterEdgeLabel";
import ParameterEdgeLabelByCollection from "./ParameterEdgeLabelByCollection";

export const AccordionEdgesContent = () => {
  return (
    <Grid
      rowGap="2"
      alignItems="center"
      templateColumns="150px 1fr 40px"
      justifyItems="start"
    >
      <ParameterEdgeLabel />
      <ParameterEdgeColor />
      <ParameterEdgeColorByCollection />
      <ParameterEdgeColorAttribute />
      <ParameterEdgeLabelByCollection />
      <ParameterEdgeDirection />
      <EdgeStyleSelector />
    </Grid>
  );
};
