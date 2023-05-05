import {
  Modal as ChakraModal,
  ModalContent,
  ModalContentProps,
  ModalOverlay,
  ModalProps
} from "@chakra-ui/react";
import React from "react";

export const Modal = (
  modalProps: ModalProps & { modalContentProps?: ModalContentProps }
) => {
  const { children, modalContentProps, ...rest } = modalProps;
  return (
    <ChakraModal {...rest}>
      <ModalOverlay />
      <ModalContent marginX="4" {...modalContentProps}>
        {children}
      </ModalContent>
    </ChakraModal>
  );
};
