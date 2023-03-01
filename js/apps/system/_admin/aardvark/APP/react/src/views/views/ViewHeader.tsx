import React from "react";
import { Cell } from "../../components/pure-css/grid";
import CopyFromInput from "./forms/inputs/CopyFromInput";
import { DeleteButton, SaveButton } from "./Actions";
import { EditableViewName } from "./EditableViewName";
import { FormState } from "./constants";
import { FormProps } from "../../utils/constants";

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
} & Pick<FormProps<FormState>, 'dispatch'>) => {
  return (
    <div className="viewsstickyheader">
      <EditableViewName
        editName={editName}
        formState={formState}
        handleEditName={handleEditName}
        updateName={updateName}
        nameEditDisabled={nameEditDisabled}
        closeEditName={closeEditName}
      />
      {isAdminUser && views.length ? (
        <Cell size={"1"} style={{ paddingLeft: 10 }}>
          <div style={{ display: "flex" }}>
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
              <DeleteButton
                view={formState}
              />
            </div>
          </div>
        </Cell>
      ) : null}
    </div>
  );
};
