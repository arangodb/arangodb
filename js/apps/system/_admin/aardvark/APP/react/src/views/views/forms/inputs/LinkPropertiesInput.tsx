import { FormProps } from "../../../../utils/constants";
import { LinkProperties } from "../../constants";
import React, { ChangeEvent, useEffect, useMemo, useState } from "react";
import { chain, get, without } from "lodash";
import { Cell, Grid } from "../../../../components/pure-css/grid";
import Checkbox from "../../../../components/pure-css/form/Checkbox";
import { getBooleanFieldSetter } from "../../../../utils/helpers";
import AutoCompleteMultiSelect from "../../../../components/pure-css/form/AutoCompleteMultiSelect";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../../../utils/arangoClient";
import FieldList from "../../Components/FieldList";
import ToolTip from "../../../../components/arango/tootip";

type LinkPropertiesInputProps = FormProps<LinkProperties> & {
  basePath: string;
};

const LinkPropertiesInput = ({
                               formState,
                               dispatch,
                               disabled,
                               basePath
                             }: LinkPropertiesInputProps) => {
  const { data } = useSWR("/analyzer", path =>
    getApiRouteForCurrentDB().get(path)
  );
  const [options, setOptions] = useState<string[]>([]);

  const analyzers = useMemo(() => get(formState, 'analyzers', [] as string[]), [formState]);

  useEffect(() => {
    if (data) {
      const tempOptions = chain(data.body.result)
        .map("name")
        .difference(analyzers)
        .value();

      setOptions(tempOptions);
    }
  }, [analyzers, data]);

  const addAnalyzer = (analyzer: string | number) => {
    dispatch({
      type: "setField",
      field: {
        path: "analyzers",
        value: analyzers.concat([analyzer as string])
      },
      basePath
    });
    setOptions(without(options, analyzer as string));
  };

  const removeAnalyzer = (analyzer: string | number) => {
    dispatch({
      type: "setField",
      field: {
        path: "analyzers",
        value: without(analyzers, analyzer)
      },
      basePath
    });
    setOptions(options.concat([analyzer as string]).sort());
  };

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
      <Cell size={"1"}>
        <Grid style={{ marginTop: 24 }}>
          <Cell size={"1-3"} style={{ marginBottom: 24 }}>
            <AutoCompleteMultiSelect
              values={analyzers}
              onRemove={removeAnalyzer}
              onSelect={addAnalyzer}
              options={options}
              label={"Analyzers"}
              disabled={disabled}
              errorMsg={'Analyzer does not exist.'}
            />
          </Cell>
          <Cell size={"1-3"}>
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
            </Grid>
          </Cell>
          <Cell size={"1-3"}>
            <Grid>
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
      </Cell>

      <Cell size={"1"}>
        <FieldList fields={formState.fields} disabled={disabled} basePath={basePath}/>
      </Cell>
    </Grid>
  );
};

export default LinkPropertiesInput;
