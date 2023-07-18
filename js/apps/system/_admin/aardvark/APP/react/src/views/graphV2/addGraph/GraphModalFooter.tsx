import { Button, Stack } from "@chakra-ui/react";
import { useFormikContext } from "formik";
import React from "react";
import { ModalFooter } from "../../../components/modal";
import { useGraphsModeContext } from "../listGraphs/GraphsModeContext";
import { EditGraphButtons } from "../listGraphs/EditGraphButtons";

export const GraphModalFooter = ({ onClose }: { onClose: () => void; }) => {
  const { initialGraph, mode } = useGraphsModeContext();
  const { isSubmitting } = useFormikContext();
  return (
    <ModalFooter>
      <Stack direction="row" spacing={4} align="center">
        {mode === "edit" && (
          <EditGraphButtons graph={initialGraph} onClose={onClose} />
        )}
        <Button onClick={onClose} colorScheme="gray">
          Cancel
        </Button>
        {mode === "add" && (
          <Button colorScheme="blue" type="submit" isLoading={isSubmitting}>
            Create
          </Button>
        )}
      </Stack>
    </ModalFooter>
  );
};
