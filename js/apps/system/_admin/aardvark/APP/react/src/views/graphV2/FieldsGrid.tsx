import { Box } from "@chakra-ui/react";
import React from "react";

export const FieldsGrid = ({
  children,
  backgroundColor,
  verticalPadding
}: {
  children: React.ReactNode;
  backgroundColor?: string;
  verticalPadding?: string;
}) => {
  return (
    <Box
      display={"grid"}
      gridTemplateColumns={"200px 1fr 40px"}
      rowGap="5"
      columnGap="3"
      maxWidth="800px"
      paddingTop={verticalPadding}
      paddingBottom={verticalPadding}
      paddingRight="8"
      paddingLeft="4"
      alignItems="center"
      marginTop="4"
      bg={backgroundColor}
    >
      {children}
    </Box>
  );
};
