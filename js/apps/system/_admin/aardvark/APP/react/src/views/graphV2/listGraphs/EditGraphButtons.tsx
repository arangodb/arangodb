import { Button, useDisclosure } from "@chakra-ui/react";
import { GraphInfo } from "arangojs/graph";
import React from "react";
import { DeleteGraphModal } from "./DeleteGraphModal";

export const EditGraphButtons = ({ graph }: { graph?: GraphInfo }) => {
  const { isOpen, onOpen, onClose } = useDisclosure();
  return (
    <>
      <DeleteGraphModal isOpen={isOpen} onClose={onClose} graph={graph} />
      <Button colorScheme="red" type="button" onClick={onOpen}>
        Delete
      </Button>
      <Button
        colorScheme="orange"
        type="button"
        onClick={() => console.log("Reset display settings")}
      >
        Reset display settings
      </Button>
    </>
  );
};
