import { Tag, VStack } from "@chakra-ui/react";
import React from "react";

export const AttributesInfo = ({
  attributes
}: {
  attributes?: Partial<{ [key: string]: string }>;
}) => {
  if (!attributes) {
    return null;
  }
  return (
    <VStack alignItems="start" spacing={2}>
      {Object.keys(attributes).map(key => (
        <Tag key={key} wordBreak="break-all">{`${key}: ${JSON.stringify(attributes[key])}`}</Tag>
      ))}
    </VStack>
  );
};
