import { Box, Tag } from "@chakra-ui/react";
import React from "react";

export const AttributesInfo = ({
  attributes
}: {
  attributes?: { [key: string]: string; };
}) => {
  if (!attributes) {
    return null;
  }
  return (
    <Box>
      {Object.keys(attributes).map(key => (
        <Tag key={key}>{`${key}: ${JSON.stringify(attributes[key])}`}</Tag>
      ))}
    </Box>
  );
};
