import { Box } from "@chakra-ui/react";
import React from "react";
import { FormProps } from "../../utils/constants";
import { DeleteButton, SaveButton } from "./Actions";
import { FormState } from "./constants";
import { EditableViewName } from "./EditableViewName";
import CopyFromInput from "./forms/inputs/CopyFromInput";

export const ViewHeader = ({
  editName,
  formState,
  handleEditName,
  updateName,
  nameEditDisabled,
  closeEditName,
  isAdminUser,
  views,
  dispatch,
  changed,
  name,
  setChanged
}: {
  editName: boolean;
  formState: FormState;
  handleEditName: () => void;
  updateName: (event: React.ChangeEvent<HTMLInputElement>) => void;
  nameEditDisabled: boolean;
  closeEditName: () => void;
  isAdminUser: boolean;
  views: FormState[];
  changed: boolean;
  name: string;
  setChanged: (changed: boolean) => void;
} & Pick<FormProps<FormState>, "dispatch">) => {
  return (
    <Box
      position="sticky"
      top="0"
      backgroundColor="gray.100"
      borderBottom="2px solid"
      borderColor={"gray.300"}
      padding="4"
      zIndex="900"
    >
      <EditableViewName
        isAdminUser={isAdminUser}
        editName={editName}
        formState={formState}
        handleEditName={handleEditName}
        updateName={updateName}
        nameEditDisabled={nameEditDisabled}
        closeEditName={closeEditName}
      />
      {isAdminUser && views.length ? (
        <Box display="flex">
          <CopyFromInput
            views={views}
            dispatch={dispatch}
            formState={formState}
          />
          <Box display="flex" ml="auto" flexWrap="wrap">
            <SaveButton
              view={formState}
              oldName={name}
              menu={"json"}
              setChanged={setChanged}
              disabled={!isAdminUser || !changed}
            />
            <DeleteButton view={formState} />
          </Box>
        </Box>
      ) : null}
    </Box>
  );
};
