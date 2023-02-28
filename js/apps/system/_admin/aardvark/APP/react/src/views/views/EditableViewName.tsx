import React from "react";
import Textbox from "../../components/pure-css/form/Textbox";
import { FormState } from "./constants";

export const EditableViewName = ({
  editName,
  formState,
  handleEditName,
  updateName,
  nameEditDisabled,
  closeEditName
}: {
  editName: boolean;
  formState: FormState;
  handleEditName: () => void;
  updateName: (event: Event) => void;
  nameEditDisabled: boolean;
  closeEditName: () => void;
}) => {
  return (
    <div>
      {!editName ? (
        <>
          <div
            style={{
              color: "#717d90",
              fontWeight: 600,
              fontSize: "12.5pt",
              padding: 10,
              float: "left"
            }}
          >
            {formState.name}
          </div>
          {!nameEditDisabled ? (
            <i
              className="fa fa-edit"
              onClick={handleEditName}
              style={{ paddingTop: "14px" }}
            ></i>
          ) : null}
        </>
      ) : (
        <>
          <Textbox
            type={"text"}
            value={formState.name}
            onChange={updateName}
            required={true}
            disabled={nameEditDisabled}
          />{" "}
          <i
            className="fa fa-check"
            onClick={closeEditName}
            style={{ paddingTop: "14px" }}
          ></i>
        </>
      )}
    </div>
  );
};
