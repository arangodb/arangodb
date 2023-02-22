import React from "react";
import Textarea from "../../components/pure-css/form/Textarea";
import { Cell, Grid } from "../../components/pure-css/grid";
import JsonForm from "./forms/JsonForm";
import { FormState } from "./constants";
import { State } from "../../utils/constants";

export const ViewRightPane = ({
  isAdminUser,
  formState,
  dispatch,
  state
}: {
  isAdminUser: boolean;
  formState: FormState;
  dispatch: () => void;
  state: State<FormState>;
}) => {
  let jsonFormState = "";
  let jsonRows = 1;
  if (!isAdminUser) {
    jsonFormState = JSON.stringify(formState, null, 4);
    jsonRows = jsonFormState.split("\n").length;
  }

  return (
    <div style={{ marginLeft: "15px" }}>
      <div
        id={"modal-dialog"}
        className={"createModalDialog"}
        tabIndex={-1}
        role={"dialog"}
        aria-labelledby={"myModalLabel"}
        aria-hidden={"true"}
        style={{
          marginLeft: "auto",
          marginRight: "auto"
        }}
      >
        <div className="modal-body" id={"view-json"}>
          <div className={"tab-content"} style={{ display: "unset" }}>
            <div className="tab-pane tab-pane-modal active" id="JSON">
              <Grid>
                <Cell size={"1"}>
                  {isAdminUser ? (
                    <JsonForm
                      formState={formState}
                      dispatch={dispatch}
                      renderKey={state.renderKey}
                    />
                  ) : (
                    <Textarea
                      disabled={true}
                      value={jsonFormState}
                      rows={jsonRows}
                      style={{
                        cursor: "text",
                        marginTop: "0",
                        marginBottom: "0",
                        width: "100%",
                        height: "95vh",
                        paddingLeft: "0",
                        paddingRight: "0",
                        border: "0"
                      }}
                    />
                  )}
                </Cell>
              </Grid>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
