import React from "react";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../components/modal";
import { noop } from "lodash";
import { FormState } from "./constants";
import { Cell, Grid } from "../../components/pure-css/grid";
import BaseForm from "./forms/BaseForm";
import FeatureForm from "./forms/FeatureForm";
import { getForm } from "./helpers";
import Textarea from "../../components/pure-css/form/Textarea";
import { Text } from "@chakra-ui/react";

export function ViewAnalyzerModal({
  isOpen,
  onClose,
  formState,
  toggleJsonForm,
  showJsonForm,
  jsonFormState,
  jsonRows
}: {
  isOpen: boolean;
  onClose: () => void;
  formState: FormState;
  toggleJsonForm: () => void;
  showJsonForm: boolean;
  jsonFormState: string;
  jsonRows: number;
}) {
  return (
    <Modal size="6xl" isOpen={isOpen} onClose={onClose}>
      <ModalHeader>
        <Text>{formState.name}</Text>
        <button
          className={"button-info"}
          onClick={toggleJsonForm}
          style={{ float: "right" }}
        >
          {showJsonForm ? "Switch to form view" : "Switch to code view"}
        </button>
      </ModalHeader>
      <ModalBody>
        <Grid>
          {showJsonForm ? (
            <Cell size={"1"}>
              <Textarea
                label={"JSON Dump"}
                disabled={true}
                value={jsonFormState}
                rows={jsonRows}
                style={{ cursor: "text" }}
              />
            </Cell>
          ) : (
            <>
              <Cell size={"11-24"}>
                <fieldset>
                  <legend style={{ fontSize: "12pt" }}>Basic</legend>
                  <BaseForm
                    formState={formState}
                    dispatch={noop}
                    disabled={true}
                  />
                </fieldset>
              </Cell>
              <Cell size={"1-12"} />
              <Cell size={"11-24"}>
                <fieldset>
                  <legend style={{ fontSize: "12pt" }}>Features</legend>
                  <FeatureForm
                    formState={formState}
                    dispatch={noop}
                    disabled={true}
                  />
                </fieldset>
              </Cell>

              {formState.type === "identity" ? null : (
                <Cell size={"1"}>
                  <fieldset>
                    <legend style={{ fontSize: "12pt" }}>Configuration</legend>
                    {getForm({
                      formState,
                      dispatch: noop,
                      disabled: true
                    })}
                  </fieldset>
                </Cell>
              )}
            </>
          )}
        </Grid>
      </ModalBody>
      <ModalFooter>
        <button className="button-close" onClick={onClose}>
          Close
        </button>
      </ModalFooter>
    </Modal>
  );
}
