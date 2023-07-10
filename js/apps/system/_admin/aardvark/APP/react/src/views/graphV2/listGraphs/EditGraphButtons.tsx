import { Button, useDisclosure } from "@chakra-ui/react";
import { GraphInfo } from "arangojs/graph";
import React from "react";
import { DeleteGraphModal } from "./DeleteGraphModal";
import { fetchUserConfig } from "../useFetchAndSetupGraphParams";
import { DEFAULT_URL_PARAMETERS } from "../UrlParametersContext";
import { putUserConfig } from "../useGraphSettingsHandlers";

export const EditGraphButtons = ({ graph }: { graph?: GraphInfo }) => {
  const { isOpen, onOpen, onClose } = useDisclosure();
  const resetDisplaySettings = async () => {
    const fullConfig = await fetchUserConfig();
    const info = await putUserConfig({
      params: DEFAULT_URL_PARAMETERS,
      fullConfig,
      graphName: "routeplanner" // currently hardcoded: To be changed
      //graphName: graph?.name
    });
    return info;
  };
  return (
    <>
      <DeleteGraphModal isOpen={isOpen} onClose={onClose} graph={graph} />
      <Button colorScheme="red" type="button" onClick={onOpen}>
        Delete
      </Button>
      <Button colorScheme="orange" type="button" onClick={resetDisplaySettings}>
        Reset display settings
      </Button>
    </>
  );
};
