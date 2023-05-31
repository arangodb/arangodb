import { CheckIcon } from "@chakra-ui/icons";
import { Button } from "@chakra-ui/react";
import React from "react";
import { FormState } from "../constants";
import { usePatchArangoSearchView } from "./usePatchArangoSearchView";

type SaveButtonProps = {
  view: FormState;
  disabled?: boolean;
  oldName?: string;
  setChanged: (changed: boolean) => void;
};

export const SaveArangoSearchViewButton = ({
  disabled,
  view,
  oldName,
  setChanged
}: SaveButtonProps) => {
  const handleSave = usePatchArangoSearchView(view, oldName, setChanged);

  return (
    <Button
      size="xs"
      colorScheme="green"
      leftIcon={<CheckIcon />}
      onClick={handleSave}
      isDisabled={disabled}
      marginRight="3"
    >
      Save view
    </Button>
  );
};
