import { Button, Stack } from "@chakra-ui/react";
import { useFormikContext } from "formik";
import React from "react";
import { ModalFooter } from "../../../components/modal";
import { EditGraphButtons } from "../listGraphs/EditGraphButtons";
import { useGraphsModeContext } from "../listGraphs/GraphsModeContext";

export const GraphModalFooter = ({ onClose }: { onClose: () => void }) => {
  const { initialGraph, mode } = useGraphsModeContext();
  const { isSubmitting } = useFormikContext();
  return (
    <ModalFooter>
      <Stack direction="row" spacing={4} align="center">
        {mode === "edit" && (
          <EditGraphButtons graph={initialGraph} onClose={onClose} />
        )}
        <Button size="sm" onClick={onClose} colorScheme="gray">
          Cancel
        </Button>
        <Button size="sm" colorScheme="green" type="submit" isLoading={isSubmitting}>
          {mode === "add" ? "Create" : "Save"}
        </Button>
      </Stack>
    </ModalFooter>
  );
};
