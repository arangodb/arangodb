import { AddIcon } from "@chakra-ui/icons";
import { Box, Button, Stack, Text, useDisclosure } from "@chakra-ui/react";
import React from "react";
import { AddNewViewModal } from "./addNewViewForm/AddNewViewModal";

export const AddViewTile = () => {
  const { onOpen, onClose, isOpen } = useDisclosure();
  return (
    <Box height="100px">
      <Button
        width={"full"}
        height={"full"}
        boxShadow="md"
        backgroundColor="white"
        onClick={onOpen}
      >
        <Stack direction={"row"} width={"full"}>
          <AddIcon />
          <Text>Add View</Text>
        </Stack>
      </Button>
      <AddNewViewModal onClose={onClose} isOpen={isOpen} />
    </Box>
  );
};

