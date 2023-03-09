import { Box } from "@chakra-ui/react";
import React from "react";
import { Cell, Grid } from "../../components/pure-css/grid";
import { FormDispatch, State } from "../../utils/constants";
import { FormState } from "./constants";
import JsonForm from "./forms/JsonForm";

export const ViewRightPane = ({
  formState,
  dispatch,
  state
}: {
  formState: FormState;
  dispatch: FormDispatch<FormState>
  state: State<FormState>;
}) => {
  return (
    <Box padding="4">
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
                  <JsonForm
                    formState={formState}
                    dispatch={dispatch}
                    renderKey={state.renderKey}
                    mode={"code"}
                  />
                </Cell>
              </Grid>
            </div>
          </div>
        </div>
      </div>
    </Box>
  );
};
