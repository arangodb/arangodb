import {
  Modal as ChakraModal,
  ModalContent,
  ModalOverlay,
  ModalProps
} from "@chakra-ui/react";
import React from "react";

export const Modal = (modalProps: ModalProps) => {
  const { children, ...rest } = modalProps;
  return (
    <ChakraModal {...rest}>
      <ModalOverlay />
      <ModalContent marginX="4">{children}</ModalContent>
    </ChakraModal>
  );
};
