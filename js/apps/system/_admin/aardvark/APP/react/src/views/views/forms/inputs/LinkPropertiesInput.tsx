import { FormProps } from "../../../../utils/constants";
import { LinkProperties } from "../../constants";
import React, { ChangeEvent, useEffect, useMemo, useState } from "react";
import { chain, get, isEmpty, without } from "lodash";
import { Cell, Grid } from "../../../../components/pure-css/grid";
import Checkbox from "../../../../components/pure-css/form/Checkbox";
import Fieldset from "../../../../components/pure-css/form/Fieldset";
import { getBooleanFieldSetter } from "../../../../utils/helpers";
import AutoCompleteMultiSelect from "../../../../components/pure-css/form/AutoCompleteMultiSelect";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../../../utils/arangoClient";
import FieldList from "../../Components/FieldList";

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
      <Cell size={"1-1"}>
        <Grid style={{ marginTop: 24 }}>
          <Cell size={"1-3"} style={{
            marginBottom: 24,
            marginLeft: 10
          }}>
            <AutoCompleteMultiSelect
              values={analyzers}
              onRemove={removeAnalyzer}
              onSelect={addAnalyzer}
              options={options}
              label={"Analyzers"}
              disabled={disabled}
            />
          </Cell>
          <Cell size={"1-1"} style={{
            margingTop: 24,
            marginLeft: 10
          }}>
            <Cell size={hideInBackgroundField ? "1-5" : "1-6"}>
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
            <Cell size={hideInBackgroundField ? "1-5" : "1-6"}>
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
            <Cell size={hideInBackgroundField ? "1-5" : "1-6"}>
              <Checkbox
                onChange={updateStoreValues}
                label={"Store ID Values"}
                disabled={disabled}
                inline={true}
                checked={storeIdValues}
              />
            </Cell>
            {hideInBackgroundField ? null : (
              <Cell size={"1-6"}>
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
            )}
          </Cell>
        </Grid>
      </Cell>

      <Cell size={"1"}>
        <Fieldset legend={"Fields"}>
          {
            isEmpty(formState.fields)
              ? null
              : <FieldList fields={formState.fields} disabled={disabled} basePath={basePath}/>
          }
        </Fieldset>
      </Cell>
    </Grid>
  );
};

export default LinkPropertiesInput;
