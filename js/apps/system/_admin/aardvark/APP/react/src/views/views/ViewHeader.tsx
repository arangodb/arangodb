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
        <Box>
          <Box display={"flex"}>
            <div style={{ flex: 1, padding: "10" }}>
              <CopyFromInput
                views={views}
                dispatch={dispatch}
                formState={formState}
              />
            </div>
            <div style={{ flex: 1, padding: "10" }}>
              {isAdminUser && changed ? (
                <SaveButton
                  view={formState}
                  oldName={name}
                  menu={"json"}
                  setChanged={setChanged}
                />
              ) : (
                <SaveButton
                  view={formState}
                  oldName={name}
                  menu={"json"}
                  setChanged={setChanged}
                  disabled
                />
              )}
              <DeleteButton view={formState} />
            </div>
          </Box>
        </Box>
      ) : null}
    </Box>
  );
};
