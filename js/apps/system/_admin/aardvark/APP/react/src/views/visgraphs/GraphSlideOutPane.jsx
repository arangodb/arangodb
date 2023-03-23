import React from "react";
import ButtonSave from "./ButtonSave";

import Drawer from "./components/Drawer/Drawer";
import {
  Accordion,
  AccordionButton,
  AccordionIcon,
  AccordionItem,
  AccordionPanel,
  Box} from "@chakra-ui/react";
import { AccordionGraphContent } from "./AccordionGraphContent";
import { AccordionNodesContent } from "./AccordionNodesContent";
import { AccordionEdgesContent } from "./AccordionEdgesContent";

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
