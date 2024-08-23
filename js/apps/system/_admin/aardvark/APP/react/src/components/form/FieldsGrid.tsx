import { Box, BoxProps } from "@chakra-ui/react";
import React from "react";

export const FieldsGrid = ({
  children,
  ...rest
}: BoxProps & {
  children: React.ReactNode;
}) => {
  return (
    <Box
      display={"grid"}
      gridTemplateColumns={"200px 1fr 40px"}
      rowGap="5"
      columnGap="3"
      maxWidth="800px"
      paddingRight="8"
      paddingLeft="4"
      alignItems="center"
      marginTop="4"
      {...rest}
    >
      {children}
    </Box>
  );
};
