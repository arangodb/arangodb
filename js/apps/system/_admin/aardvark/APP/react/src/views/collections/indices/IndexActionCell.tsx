import { LockIcon } from "@chakra-ui/icons";
import { Box, HStack } from "@chakra-ui/react";
import React from "react";
import { DeleteIndexButton } from "./DeleteIndexButton";
import { IndexRowType } from "./useFetchIndices";
import { ViewIndexButton } from "./ViewIndexButton";

export const IndexActionCell = ({ indexRow }: { indexRow: IndexRowType }) => {
  const { type } = indexRow;
  if (type === "primary" || type === "edge") {
    return (
      <Box display="flex" justifyContent="center">
        <LockIcon />
      </Box>
    );
  }
  if (type === "inverted") {
    return (
      <HStack>
        <ViewIndexButton indexRow={indexRow} />
        <DeleteIndexButton indexRow={indexRow} />
      </HStack>
    );
  }
  return <DeleteIndexButton indexRow={indexRow} />;
};
