import { FormProps } from "../../../../utils/constants";
import { LinkProperties } from "../../constants";
import React, { ChangeEvent } from "react";
import { get } from "lodash";
import { Cell, Grid } from "../../../../components/pure-css/grid";
import Checkbox from "../../../../components/pure-css/form/Checkbox";
import { getBooleanFieldSetter } from "../../../../utils/helpers";
import ToolTip from "../../../../components/arango/tootip";
import { FieldsDropdown } from "./FieldsDropdown";
import { AnalyzersDropdown } from "./AnalyzersDropdown";

type LinkPropertiesInputProps = FormProps<LinkProperties> & {
  basePath: string;
};

const LinkPropertiesInput = ({
                               formState,
                               dispatch,
                               disabled,
                               basePath
                             }: LinkPropertiesInputProps) => {
  
  const updateStoreValues = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: "setField",
      field: {
        path: "storeValues",
        value: event.target.checked ? "id" : "none"
      },
      basePath
    });
  };


  const storeIdValues = get(formState, 'storeValues') === 'id';
  const hideInBackgroundField = disabled || basePath.includes(".fields");

  return (
    <Grid>
      <Cell size={"1-2"}>
        <Grid>
          <Cell size={"1"} style={{ marginBottom: 24 }}>
            <span
              style={{
                float: "left",
                width: "84%"
              }}
            >
              <label>Analyzers</label>
              <AnalyzersDropdown
                basePath={basePath}
                isDisabled={!!disabled}
                formState={formState}
              />
            </span>
          </Cell>

          <Cell size={"1"}>
            <span
              style={{
                float: "left",
                width: "84%"
              }}
            >
              <label>Fields</label>
              <FieldsDropdown
                basePath={basePath}
                isDisabled={!!disabled}
                formState={formState}
              />
            </span>
          </Cell>
        </Grid>
      </Cell>
      <Cell size={"1-2"}>
        <Grid>
          <Cell size={"1-2"}>
            <Checkbox
              onChange={getBooleanFieldSetter(
                "includeAllFields",
                dispatch,
                basePath
              )}
              inline={true}
              label={"Include All Fields"}
              disabled={disabled}
              checked={formState.includeAllFields}
            />
          </Cell>
          <Cell size={"1-4"}>
            <ToolTip
              title="Process all document attributes."
              setArrow={true}
            >
              <span className="arangoicon icon_arangodb_info" style={{ marginTop: 10 }}></span>
            </ToolTip>
          </Cell>
          <Cell size={"1-4"}/>
          <Cell size={"1-2"}>
            <Checkbox
              onChange={getBooleanFieldSetter(
                "trackListPositions",
                dispatch,
                basePath
              )}
              label={"Track List Positions"}
              disabled={disabled}
              inline={true}
              checked={formState.trackListPositions}
            />
          </Cell>
          <Cell size={"1-4"}>
            <ToolTip
              title="For array values track the value position in arrays."
              setArrow={true}
            >
              <span className="arangoicon icon_arangodb_info" style={{ marginTop: 10 }}></span>
            </ToolTip>
          </Cell>
          <Cell size={"1-4"}/>
          <Cell size={"1-2"}>
            <Checkbox
              onChange={updateStoreValues}
              label={"Store ID Values"}
              disabled={disabled}
              inline={true}
              checked={storeIdValues}
            />
          </Cell>
          <Cell size={"1-4"}>
            <ToolTip
              title="Store information about value presence to allow use of the EXISTS() function."
              setArrow={true}
            >
              <span className="arangoicon icon_arangodb_info" style={{ marginTop: 10 }}></span>
            </ToolTip>
          </Cell>
          <Cell size={"1-4"}/>
          {
            hideInBackgroundField
              ? null
              : <>
                <Cell size={"1-2"}>
                  <Checkbox
                    onChange={getBooleanFieldSetter(
                      "inBackground",
                      dispatch,
                      basePath
                    )}
                    label={"In Background"}
                    inline={true}
                    disabled={disabled}
                    checked={formState.inBackground}
                  />
                </Cell>
                <Cell size={"1-4"}>
                  <ToolTip
                    title="If selected, no exclusive lock is used on the source collection during View index creation."
                    setArrow={true}
                  >
                    <span className="arangoicon icon_arangodb_info" style={{ marginTop: 10 }}></span>
                  </ToolTip>
                </Cell>
                <Cell size={"1-4"}/>
              </>}
        </Grid>
      </Cell>
    </Grid>
  );
};

export default LinkPropertiesInput;
