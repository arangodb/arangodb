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
  Box
} from "@chakra-ui/react";

const AccordionGraphContent = () => {
  return (
    <>
      <ParameterNodeStart />
      <br />
      <GraphLayoutSelector />
      <ParameterDepth />
      <ParameterLimit />
    </>
  );
};

const AccordionNodesContent = () => {
  return (
    <>
      <ParameterNodeLabel />
      <br />
      <ParameterNodeColor />
      <br />
      <ParameterNodeLabelByCollection />
      <br />
      <ParameterNodeColorByCollection />
      <br />
      <ParameterNodeColorAttribute />
      <br />
      <ParameterNodeSizeByEdges />
      <br />
      <ParameterNodeSize />
    </>
  );
};

const AccordionEdgesContent = () => {
  return (
    <>
      <ParameterEdgeLabel />
      <br />
      <ParameterEdgeColor />
      <br />
      <ParameterEdgeLabelByCollection />
      <br />
      <ParameterEdgeColorByCollection />
      <br />
      <ParameterEdgeColorAttribute />
      <br />
      <ParameterEdgeDirection />
      <br />
      <EdgeStyleSelector />
    </>
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
        <div style={{ padding: "24px", display: "flex" }}>
          <ButtonSave
            graphName={graphName}
            onGraphDataLoaded={(newGraphData, responseTimesObject) => {
              onGraphDataLoaded(newGraphData, responseTimesObject);
            }}
            onIsLoadingData={isLoadingData => setIsLoadingData(isLoadingData)}
          />
        </div>
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
      </div>
    </Drawer>
  );
};

const GraphAccordionItem = ({ children, title }) => {
  return (
    <AccordionItem>
      <AccordionButton color="white">
        <Box as="span" flex="1" textAlign="left">
          {title}
        </Box>
        <AccordionIcon color="white" />
      </AccordionButton>

      <AccordionPanel pb={4}>{children}</AccordionPanel>
    </AccordionItem>
  );
};
