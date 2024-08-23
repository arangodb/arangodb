import {
  Box, useDisclosure
} from "@chakra-ui/react";
import React from "react";
import { ListHeader } from "../../../components/table/ListHeader";
import { AddGraphModal } from "../addGraph/AddGraphModal";
import { GraphsListProvider } from "./GraphsListContext";
import { GraphsModeProvider } from "./GraphsModeContext";
import { GraphsTable } from "./GraphsTable";

export const GraphsListView = () => {
  const { isOpen, onOpen, onClose } = useDisclosure();
  return (
    <Box padding="4" width="100%">
      <GraphsListProvider>
        <GraphListViewHeader onOpen={onOpen} />
        <GraphsModeProvider mode="add">
          <AddGraphModal isOpen={isOpen} onClose={onClose} />
        </GraphsModeProvider>
        <GraphsTable />
      </GraphsListProvider>
    </Box>
  );
};

const GraphListViewHeader = ({ onOpen }: { onOpen: () => void }) => {
  return (
    <ListHeader
      onButtonClick={onOpen}
      heading="Graphs"
      buttonText="Add graph"
    />
  );
};
