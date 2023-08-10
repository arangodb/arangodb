import { Grid } from "@chakra-ui/react";
import React from "react";
import GraphLayoutSelector from "./GraphLayoutSelector";
import ParameterDepth from "./ParameterDepth";
import ParameterLimit from "./ParameterLimit";
import ParameterNodeStart from "./ParameterNodeStart";

export const AccordionGraphContent = () => {
  return (
    <Grid
      rowGap="2"
      alignItems="center"
      templateColumns="150px 1fr 40px"
      justifyItems="start"
    >
      <ParameterNodeStart />
      <GraphLayoutSelector />
      <ParameterDepth />
      <ParameterLimit />
    </Grid>
  );
};
