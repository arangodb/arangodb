import { FormProps } from "../../../../utils/constants";
import { LinkProperties } from "../../constants";
import React, {
  ChangeEvent,
  useContext,
  useEffect,
  useMemo,
  useState
} from "react";
import { chain, isEmpty, without } from "lodash";
import { Cell, Grid } from "../../../../components/pure-css/grid";
import Checkbox from "../../../../components/pure-css/form/Checkbox";
import { ArangoTable, ArangoTD } from "../../../../components/arango/table";
import Textbox from "../../../../components/pure-css/form/Textbox";
import { IconButton } from "../../../../components/arango/buttons";
import { useLinkState } from "../../helpers";
import Fieldset from "../../../../components/pure-css/form/Fieldset";
import { getBooleanFieldSetter } from "../../../../utils/helpers";
import AutoCompleteMultiSelect from "../../../../components/pure-css/form/AutoCompleteMultiSelect";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../../../utils/arangoClient";
import FieldList from "../../Components/FieldList";
import { ViewContext } from "../../ViewLinksReactView";

type LinkPropertiesInputProps = FormProps<LinkProperties> & {
  basePath: string;
};

const LinkPropertiesInput = ({
  formState,
  dispatch,
  disabled,
  basePath
}: LinkPropertiesInputProps) => {
  const [field, setField, addDisabled, fields] = useLinkState(
    formState,
    "fields"
  );
  const { data } = useSWR("/analyzer", path =>
    getApiRouteForCurrentDB().get(path)
  );
  const [options, setOptions] = useState<string[]>([]);

  const analyzers = useMemo(() => formState.analyzers || [], [
    formState.analyzers
  ]);

  const { setShow, setField: setCurrentField } = useContext(ViewContext);

  useEffect(() => {
    if (data) {
      const tempOptions = chain(data.body.result)
        .map("name")
        .difference(analyzers)
        .value();

      setOptions(tempOptions);
    }
  }, [analyzers, data]);

  const updateField = (event: ChangeEvent<HTMLInputElement>) => {
    setField(event.target.value);
  };

  const addField = () => {
    dispatch({
      type: "setField",
      field: {
        path: `fields[${field}]`,
        value: {}
      },
      basePath
    });
    setField("");
  };

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

  // const removeField = (field: string | number) => {
  //   dispatch({
  //     type: "unsetField",
  //     field: {
  //       path: `fields[${field}]`
  //     },
  //     basePath
  //   });
  // };

  // const getFieldRemover = (field: string | number) => () => {
  //   removeField(field);
  // };
  const handleShowField = (field: string) => {
    setCurrentField({
      field: field,
      basePath: basePath
    });
    setShow("ViewField");
  };

  const storeIdValues = formState.storeValues === "id";
  const hideInBackgroundField = disabled || basePath.includes(".fields");
  return (
    <Grid>
      <Cell size={"1-1"}>
        <Grid>
          <Cell size={"1-1"}>
            {disabled ? null : (
              <ArangoTable>
                <tbody>
                  <tr>
                    <ArangoTD seq={0} colSpan={2}>
                      <Textbox
                        type={"text"}
                        placeholder={"Field"}
                        onChange={updateField}
                        value={field}
                      />
                    </ArangoTD>
                    <ArangoTD seq={1}>
                      <IconButton
                        style={{ marginTop: "12px" }}
                        icon={"plus"}
                        type={"warning"}
                        onClick={addField}
                        disabled={addDisabled}
                      >
                        Add
                      </IconButton>
                    </ArangoTD>
                  </tr>
                </tbody>
              </ArangoTable>
            )}
          </Cell>
        </Grid>
      </Cell>

      <Cell size={"1-1"}>
        <Grid style={{ marginTop: 24 }}>
          <Cell size={"1-3"} style={{ marginBottom: 24, marginLeft: 10 }}>
            <AutoCompleteMultiSelect
              values={analyzers}
              onRemove={removeAnalyzer}
              onSelect={addAnalyzer}
              options={options}
              label={"Analyzers"}
              disabled={disabled}
            />
          </Cell>
          <Cell size={"1-1"} style={{ margingTop: 24, marginLeft: 10 }}>
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
          {disabled && isEmpty(fields) ? null : (
            <FieldList
              fields={fields}
              disabled={disabled}
              basePath={basePath}
              viewField={handleShowField}
            />
          )}
        </Fieldset>

        {/* {map(fields, (properties, fld) => {
              return (
                <tr key={fld} style={{ borderBottom: "1px  solid #DDD" }}>
                  <ArangoTD seq={disabled ? 0 : 1}>{fld}</ArangoTD>
                  <ArangoTD seq={disabled ? 1 : 2}>
                    <LinkPropertiesInput
                      formState={properties}
                      disabled={disabled}
                      basePath={`${basePath}.fields[${fld}]`}
                      dispatch={
                        (dispatch as unknown) as Dispatch<
                          DispatchArgs<LinkProperties>
                        >
                      }
                    />
                  </ArangoTD>
                  {disabled ? null : (
                    <ArangoTD seq={0} valign={"middle"}>
                      <IconButton
                        icon={"trash-o"}
                        type={"danger"}
                        onClick={getFieldRemover(fld)}
                      />
                      <IconButton
                        icon={"eye"}
                        type={"warning"}
                        onClick={getFieldRemover(fld)}
                      />
                    </ArangoTD>
                  )}
                </tr>
              );
            })} */}
      </Cell>
    </Grid>
  );
};

export default LinkPropertiesInput;
