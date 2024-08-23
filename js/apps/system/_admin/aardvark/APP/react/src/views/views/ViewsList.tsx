import { Box, useDisclosure } from "@chakra-ui/react";
import React from "react";
import { ListHeader } from "../../components/table/ListHeader";
import { AddNewViewModal } from "./viewsList/addNewViewForm/AddNewViewModal";
import { ViewsTable } from "./viewsList/ViewsTable";

export const ViewsList = () => {
  const { isOpen, onOpen, onClose } = useDisclosure();
  return (
    <Box padding="4" width="100%">
      <ViewListHeader onOpen={onOpen} />
      <ViewsTable />
      <AddNewViewModal isOpen={isOpen} onClose={onClose} />
    </Box>
  );
};

const ViewListHeader = ({ onOpen }: { onOpen: () => void }) => {
  return (
    <ListHeader onButtonClick={onOpen} heading="Views" buttonText="Add view" />
  );
};
