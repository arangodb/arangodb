import React from "react";
import ButtonSave from "./ButtonSave";

import ParameterNodeStart from "./ParameterNodeStart";
import ParameterDepth from "./ParameterDepth";
import ParameterLimit from "./ParameterLimit";
import ParameterNodeLabelByCollection from "./ParameterNodeLabelByCollection";
import ParameterNodeColorByCollection from "./ParameterNodeColorByCollection";
import ParameterEdgeLabelByCollection from "./ParameterEdgeLabelByCollection";
import ParameterEdgeColorByCollection from "./ParameterEdgeColorByCollection";
import ParameterNodeLabel from "./ParameterNodeLabel";
import ParameterEdgeLabel from "./ParameterEdgeLabel";
import ParameterNodeColorAttribute from "./ParameterNodeColorAttribute";
import ParameterEdgeColorAttribute from "./ParameterEdgeColorAttribute";
import ParameterNodeColor from "./ParameterNodeColor";
import ParameterEdgeColor from "./ParameterEdgeColor";
import ParameterNodeSize from "./ParameterNodeSize";
import ParameterNodeSizeByEdges from "./ParameterNodeSizeByEdges";
import ParameterEdgeDirection from "./ParameterEdgeDirection";
import EdgeStyleSelector from "./EdgeStyleSelector";
import GraphLayoutSelector from "./GraphLayoutSelector";
import Drawer from "./components/Drawer/Drawer";
import {
  Accordion,
  AccordionButton,
  AccordionIcon,
  AccordionItem,
  AccordionPanel,
  Box,
  Stack
} from "@chakra-ui/react";

const AccordionGraphContent = () => {
  return (
    <Stack>
      <ParameterNodeStart />
      <GraphLayoutSelector />
      <ParameterDepth />
      <ParameterLimit />
    </Stack>
  );
};

const AccordionNodesContent = () => {
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

const AccordionEdgesContent = () => {
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

export const GraphSlideOutPane = ({
  open,
  graphName,
  onGraphDataLoaded,
  setIsLoadingData
}) => {
  return (
    <Drawer position="right" open={open}>
      <div style={{ backgroundColor: "#404a53" }}>
        <Accordion allowMultiple allowToggle defaultIndex={[0, 1, 2]}>
          <GraphAccordionItem title="Graph">
            <AccordionGraphContent />
          </GraphAccordionItem>
          <GraphAccordionItem title="Nodes">
            <AccordionNodesContent />
          </GraphAccordionItem>
          <GraphAccordionItem title="Edges">
            <AccordionEdgesContent />
          </GraphAccordionItem>
        </Accordion>

        <Box display="flex" padding="4">
          <ButtonSave
            graphName={graphName}
            onGraphDataLoaded={(newGraphData, responseTimesObject) => {
              onGraphDataLoaded(newGraphData, responseTimesObject);
            }}
            onIsLoadingData={isLoadingData => setIsLoadingData(isLoadingData)}
          />
        </Box>
      </div>
    </Drawer>
  );
};

const GraphAccordionItem = ({ children, title }) => {
  return (
    <AccordionItem border="0">
      <AccordionButton color="white" backgroundColor="#2d3338">
        <Box as="span" flex="1" textAlign="left">
          {title}
        </Box>
        <AccordionIcon color="white" />
      </AccordionButton>

      <AccordionPanel pb={4}>{children}</AccordionPanel>
    </AccordionItem>
  );
};
