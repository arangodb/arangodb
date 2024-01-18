import { AddIcon } from "@chakra-ui/icons";
import { Box, Button, Heading, Stack, useDisclosure } from "@chakra-ui/react";
import React from "react";
import { useDatabaseReadOnly } from "../../utils/useDatabaseReadOnly";
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
  const { readOnly, isLoading } = useDatabaseReadOnly();
  return (
    <Stack direction="row" marginBottom="4" alignItems="center">
      <Heading size="lg">Views</Heading>
      <Button
        size="sm"
        isLoading={isLoading}
        isDisabled={readOnly}
        leftIcon={<AddIcon />}
        colorScheme="green"
        onClick={onOpen}
      >
        Add view
      </Button>
    </Stack>
  );
};
