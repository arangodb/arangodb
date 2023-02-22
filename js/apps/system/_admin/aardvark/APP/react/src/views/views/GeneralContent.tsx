import React from "react";
import ToolTip from "../../components/arango/tootip";
import Textbox from "../../components/pure-css/form/Textbox";
import {
  getNumericFieldSetter,
  getNumericFieldValue
} from "../../utils/helpers";
import { FormState } from "./constants";

export const GeneralContent = ({
  formState,
  dispatch,
  isAdminUser
}: {
  formState: FormState;
  dispatch: () => void;
  isAdminUser: boolean;
}) => {
  return (
    <table>
      <tbody>
        <tr className="tableRow" id="row_change-view-writebufferIdle">
          <th className="collectionTh">Writebuffer Idle:</th>
          <th className="collectionTh">
            <Textbox
              type={"number"}
              disabled={true}
              value={getNumericFieldValue(formState.writebufferIdle)}
            />
          </th>
          <th className="collectionTh">
            <ToolTip
              title={`Maximum number of writers (segments) cached in the pool (default: 64, use 0 to disable, immutable).`}
              setArrow={true}
            >
              <span
                className="arangoicon icon_arangodb_info"
                style={{ fontSize: "18px", color: "rgb(85, 85, 85)" }}
              ></span>
            </ToolTip>
          </th>
        </tr>

        <tr className="tableRow" id="row_change-view-writebufferActive">
          <th className="collectionTh">Writebuffer Active:</th>
          <th className="collectionTh">
            <Textbox
              type={"number"}
              disabled={true}
              value={getNumericFieldValue(formState.writebufferActive)}
              onChange={getNumericFieldSetter("writebufferActive", dispatch)}
            />
          </th>
          <th className="collectionTh">
            <ToolTip
              title={`Maximum number of concurrent active writers (segments) that perform a transaction. Other writers (segments) wait till current active writers (segments) finish (default: 0, use 0 to disable, immutable).`}
              setArrow={true}
            >
              <span
                className="arangoicon icon_arangodb_info"
                style={{ fontSize: "18px", color: "rgb(85, 85, 85)" }}
              ></span>
            </ToolTip>
          </th>
        </tr>

        <tr className="tableRow" id="row_change-view-writebufferSizeMax">
          <th className="collectionTh">Writebuffer Size Max:</th>
          <th className="collectionTh">
            <Textbox
              type={"number"}
              disabled={true}
              value={getNumericFieldValue(formState.writebufferSizeMax)}
              onChange={getNumericFieldSetter("writebufferSizeMax", dispatch)}
            />
          </th>
          <th className="collectionTh">
            <ToolTip
              title={`Maximum memory byte size per writer (segment) before a writer (segment) flush is triggered. 0 value turns off this limit for any writer (buffer) and data will be flushed periodically based on the value defined for the flush thread (ArangoDB server startup option). 0 value should be used carefully due to high potential memory consumption (default: 33554432, use 0 to disable, immutable).`}
              setArrow={true}
            >
              <span
                className="arangoicon icon_arangodb_info"
                style={{ fontSize: "18px", color: "rgb(85, 85, 85)" }}
              ></span>
            </ToolTip>
          </th>
        </tr>

        <tr className="tableRow" id="row_change-view-cleanupIntervalStep">
          <th className="collectionTh">Cleanup Interval Step:</th>
          <th className="collectionTh">
            <Textbox
              type={"number"}
              disabled={!isAdminUser}
              min={0}
              step={1}
              value={getNumericFieldValue(formState.cleanupIntervalStep)}
              onChange={getNumericFieldSetter("cleanupIntervalStep", dispatch)}
            />
          </th>
          <th className="collectionTh">
            <ToolTip
              title={`ArangoSearch waits at least this many commits between removing unused files in its data directory.`}
              setArrow={true}
            >
              <span
                className="arangoicon icon_arangodb_info"
                style={{ fontSize: "18px", color: "rgb(85, 85, 85)" }}
              ></span>
            </ToolTip>
          </th>
        </tr>

        <tr className="tableRow" id="row_change-view-commitIntervalMsec">
          <th className="collectionTh">Commit Interval (msec):</th>
          <th className="collectionTh" style={{ width: "100%" }}>
            <Textbox
              type={"number"}
              disabled={!isAdminUser}
              min={0}
              step={1}
              value={getNumericFieldValue(formState.commitIntervalMsec)}
              onChange={getNumericFieldSetter("commitIntervalMsec", dispatch)}
            />
          </th>
          <th className="collectionTh">
            <ToolTip
              title="Wait at least this many milliseconds between committing View data store changes and making documents visible to queries."
              setArrow={true}
            >
              <span
                className="arangoicon icon_arangodb_info"
                style={{ fontSize: "18px", color: "rgb(85, 85, 85)" }}
              ></span>
            </ToolTip>
          </th>
        </tr>

        <tr className="tableRow" id="row_change-view-consolidationIntervalMsec">
          <th className="collectionTh">Consolidation Interval (msec):</th>
          <th className="collectionTh">
            <Textbox
              type={"number"}
              disabled={!isAdminUser}
              min={0}
              step={1}
              value={getNumericFieldValue(formState.consolidationIntervalMsec)}
              onChange={getNumericFieldSetter(
                "consolidationIntervalMsec",
                dispatch
              )}
            />
          </th>
          <th className="collectionTh">
            <ToolTip
              title="Wait at least this many milliseconds between index segments consolidations."
              setArrow={true}
            >
              <span
                className="arangoicon icon_arangodb_info"
                style={{ fontSize: "18px", color: "rgb(85, 85, 85)" }}
              ></span>
            </ToolTip>
          </th>
        </tr>
      </tbody>
    </table>
  );
};
