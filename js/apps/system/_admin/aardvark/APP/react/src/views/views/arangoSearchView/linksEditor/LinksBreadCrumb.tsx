import { Box, Stack } from "@chakra-ui/react";
import React from "react";
import { useEditViewContext } from "../../editView/EditViewContext";

export const LinksBreadCrumb = () => {
  const { currentLink, setCurrentLink, currentField } = useEditViewContext();
  return (
    <Stack direction="row" height="10" backgroundColor="gray.200" padding="2">
      <Box
        as="span"
        color="blue.500"
        textDecoration="underline"
        cursor="pointer"
        onClick={() => {
          setCurrentLink(undefined);
        }}
      >
        Links
      </Box>
      <Box>/</Box>
      <Box>{currentLink}</Box>
      {currentField.map((field, index) => {
        return (
          <React.Fragment key={index}>
            <Box>/</Box>
            <Box>{field}</Box>
          </React.Fragment>
        );
      })}
    </Stack>
  );
};
