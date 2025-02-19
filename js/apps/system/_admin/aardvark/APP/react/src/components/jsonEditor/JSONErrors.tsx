import { Box, BoxProps } from "@chakra-ui/react";
import { ValidationError } from "jsoneditor-react";
import React from "react";

export const JSONErrors = ({
  errors,
  ...rest
}: BoxProps & { errors?: ValidationError[] }) => {
  if (!errors || errors.length === 0) {
    return null;
  }
  return (
    <Box
      maxHeight={"80px"}
      paddingX="2"
      paddingY="1"
      fontSize="sm"
      color="red"
      background="red.100"
      overflow={"auto"}
      {...rest}
    >
      {errors.map(error => {
        return (
          <Box key={error.message}>
            {`${error.keyword ? `${error.keyword} ` : ""}Error: ${
              error.message
            }. ${
              error.params ? `Schema: ${JSON.stringify(error.params)}` : ""
            }`}
          </Box>
        );
      })}
    </Box>
  );
};
