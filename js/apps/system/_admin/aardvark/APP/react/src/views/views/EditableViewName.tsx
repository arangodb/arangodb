import { Box } from "@chakra-ui/react";
import React from "react";
import Textbox from "../../components/pure-css/form/Textbox";
import { FormState } from "./constants";

export const EditableViewName = ({
  editName,
  formState,
  handleEditName,
  updateName,
  nameEditDisabled,
  closeEditName,
  isAdminUser
}: {
  editName: boolean;
  isAdminUser: boolean;
  formState: FormState;
  handleEditName: () => void;
  updateName: (event: React.ChangeEvent<HTMLInputElement>) => void;
  nameEditDisabled: boolean;
  closeEditName: () => void;
}) => {
  if (!editName) {
    return (
      <Box display="flex" alignItems="center">
        <Box color="gray.700" paddingY="3" fontWeight="600" fontSize="lg">
          {formState.name}
          {!isAdminUser ? " (read only)" : ""}
        </Box>
        {!nameEditDisabled ? (
          <Box
            as="i"
            className="fa fa-edit"
            padding={"3"}
            onClick={handleEditName}
          />
        ) : null}
      </Box>
    );
  }
  return (
    <div>
      <>
        <Textbox
          type={"text"}
          value={formState.name}
          onChange={updateName}
          required={true}
          disabled={nameEditDisabled}
        />{" "}
        <Box
          as="i"
          className="fa fa-check"
          padding={"3"}
          onClick={closeEditName}
        />
      </>
    </div>
  );
};
