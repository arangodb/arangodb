import React, { Dispatch } from "react";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../components/modal";
import JsonForm from "./forms/JsonForm";
import FeatureForm from "./forms/FeatureForm";
import BaseForm from "./forms/BaseForm";
import { AnalyzerTypeState, FormState } from "./constants";
import { DispatchArgs, State } from "../../utils/constants";
import CopyFromInput from "./forms/inputs/CopyFromInput";
import { Cell, Grid } from "../../components/pure-css/grid";
import { getForm } from "./helpers";
import { Text } from "@chakra-ui/react";

export function AddAnalyzerModal({
  isOpen,
  onClose,
  analyzers,
  dispatch,
  formState,
  state,
  toggleJsonForm,
  handleAdd
}: {
  isOpen: boolean;
  onClose: () => void;
  state: State<FormState>;
  dispatch: React.Dispatch<DispatchArgs<FormState>>;
  analyzers: FormState[];
  toggleJsonForm: () => void;
  formState: FormState;
  handleAdd: () => Promise<void>;
}) {
  return (
    <Modal size="6xl" isOpen={isOpen} onClose={onClose}>
      <ModalHeader>
        <Grid>
          <Cell size={"2-5"}>
            <Text>Create Analyzer</Text>
          </Cell>
          <Cell size={"2-5"}>
            <CopyFromInput analyzers={analyzers} dispatch={dispatch} />
          </Cell>
          <Cell size={"1-5"}>
            <button
              className={"button-info"}
              onClick={toggleJsonForm}
              disabled={state.lockJsonForm}
              style={{
                float: "right"
              }}
            >
              {state.showJsonForm
                ? "Switch to form view"
                : "Switch to code view"}
            </button>
          </Cell>
        </Grid>
      </ModalHeader>
      <ModalBody>
        <Grid>
          {state.showJsonForm ? (
            <Cell size={"1"}>
              <JsonForm
                formState={formState}
                dispatch={dispatch}
                renderKey={state.renderKey}
              />
            </Cell>
          ) : (
            <>
              <Cell size={"11-24"}>
                <fieldset>
                  <legend style={{ fontSize: "12pt" }}>Basic</legend>
                  <BaseForm formState={formState} dispatch={dispatch} />
                </fieldset>
              </Cell>
              <Cell size={"1-12"} />
              <Cell size={"11-24"}>
                <fieldset>
                  <legend style={{ fontSize: "12pt" }}>Features</legend>
                  <FeatureForm formState={formState} dispatch={dispatch} />
                </fieldset>
              </Cell>

              {formState.type === "identity" ? null : (
                <Cell size={"1"}>
                  <fieldset>
                    <legend style={{ fontSize: "12pt" }}>Configuration</legend>
                    {getForm({
                      formState,
                      dispatch: dispatch as Dispatch<
                        DispatchArgs<AnalyzerTypeState>
                      >,
                      disabled: false
                    })}
                  </fieldset>
                </Cell>
              )}
            </>
          )}
        </Grid>
      </ModalBody>
      <ModalFooter>
        <button
          className="button-close"
          onClick={() => dispatch({ type: "reset" })}
        >
          Close
        </button>
        <button
          className="button-success"
          style={{ float: "right" }}
          onClick={handleAdd}
          disabled={state.lockJsonForm}
        >
          Create
        </button>
      </ModalFooter>
    </Modal>
  );
}
