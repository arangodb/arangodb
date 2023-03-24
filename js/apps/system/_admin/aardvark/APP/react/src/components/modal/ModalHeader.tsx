import {
  ModalCloseButton,
  ModalHeader as ChakraModalHeader,
  ModalHeaderProps
} from "@chakra-ui/react";
import React from "react";

export const ModalHeader = (
  props: ModalHeaderProps & { showClose?: boolean }
) => {
  const { showClose, ...rest } = props;
  return (
    <>
      <ChakraModalHeader {...rest} />
      {showClose ? <ModalCloseButton /> : null}
    </>
  );
};
