import { Button, useDisclosure } from "@chakra-ui/react";
import { GraphInfo } from "arangojs/graph";
import React from "react";
import { notifyError, notifySuccess } from "../../../utils/notifications";
import { DEFAULT_URL_PARAMETERS } from "../viewGraph/UrlParametersContext";
import { fetchUserConfig } from "../viewGraph/useFetchAndSetupGraphParams";
import { putUserConfig } from "../viewGraph/useGraphSettingsHandlers";
import { DeleteGraphModal } from "./DeleteGraphModal";

export const EditGraphButtons = ({
  graph,
  onClose
}: {
  graph?: GraphInfo;
  onClose: () => void;
}) => {
  const {
    isOpen: isDeleteModalOpen,
    onOpen: onOpenDeleteModal,
    onClose: onCloseDeleteModal
  } = useDisclosure();
  const resetDisplaySettings = async () => {
    try {
      const fullConfig = await fetchUserConfig();
      if (!graph?.name) {
        return;
      }
      const info = await putUserConfig({
        params: DEFAULT_URL_PARAMETERS,
        fullConfig,
        graphName: graph.name
      });
      notifySuccess(
        `Successfully reset display settings for graph "${graph.name}"`
      );
      onClose();
      return info;
    } catch (e: any) {
      const errorMessage =
        e?.response?.parsedBody?.errorMessage || "Unknown error";
      notifyError(`Could not reset graph display settings: ${errorMessage}`);
    }
  };
  return (
    <>
      <DeleteGraphModal
        isOpen={isDeleteModalOpen}
        onClose={onCloseDeleteModal}
        onSuccess={onClose}
        currentGraph={graph}
      />
      <Button
        size="sm"
        colorScheme="red"
        type="button"
        onClick={onOpenDeleteModal}
      >
        Delete
      </Button>
      <Button
        size="sm"
        colorScheme="gray"
        type="button"
        onClick={resetDisplaySettings}
      >
        Reset display settings
      </Button>
    </>
  );
};
