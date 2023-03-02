import { AddIcon } from "@chakra-ui/icons";
import { Box, Button, Stack, Text } from "@chakra-ui/react";
import React from "react";

export const AddViewTile = () => {
  return (
    <Box>
      <Button
        height="100px"
        boxShadow="md"
        backgroundColor="white"
      >
        <Stack>
          <AddIcon />
          <Text>Add View</Text>
        </Stack>
      </Button>
    </Box>
  );
};
