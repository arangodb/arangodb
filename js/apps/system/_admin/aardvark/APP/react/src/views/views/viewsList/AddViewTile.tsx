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
        leftIcon={<AddIconCircle />}
      >
        <Stack direction={"row"} width={"full"}>
          <Text>Add View</Text>
        </Stack>
      </Button>
      <AddNewViewModal onClose={onClose} isOpen={isOpen} />
    </Box>
  );
};

const AddIconCircle = () => {
  return (
    <Box
      background="green.400"
      width="5"
      height="5"
      borderRadius="full"
      display="flex"
      alignItems="center"
      justifyContent="center"
      color="white"
    >
      <AddIcon boxSize="3" />
    </Box>
  );
};
