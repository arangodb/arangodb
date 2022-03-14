import { FormProps } from "../../../../utils/constants";
import { LinkProperties } from "../../constants";
import React, {
  Dispatch,
  ChangeEvent,
  useEffect,
  useMemo,
  useState
} from "react";
import { chain, isEmpty, without } from "lodash";
import { Cell, Grid } from "../../../../components/pure-css/grid";
import Checkbox from "../../../../components/pure-css/form/Checkbox";
import { DispatchArgs } from "../../../../utils/constants";
import { ArangoTD } from "../../../../components/arango/table";
import Textbox from "../../../../components/pure-css/form/Textbox";
import { IconButton } from "../../../../components/arango/buttons";
import { useLinkState } from "../../helpers";
import Fieldset from "../../../../components/pure-css/form/Fieldset";
import { getBooleanFieldSetter } from "../../../../utils/helpers";
import AutoCompleteMultiSelect from "../../../../components/pure-css/form/AutoCompleteMultiSelect";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../../../utils/arangoClient";
import FieldList from "../../Components/FieldList";
import FieldView from "../../Components/FieldView";

type LinkPropertiesInputProps = FormProps<LinkProperties> & {
  basePath: string;
};

const LinkPropertiesInput = ({
  formState,
  dispatch,
  disabled,
  basePath,
  view
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

  const [showChild, setShowChild] = useState(false);
  const [fieldName, setFieldName] = useState<string>();
  const [childBasePath, setBasePath] = useState("");

  const handleShowField = (field: string, basePath: string) => {
    setShowChild(true);
    setFieldName(field);
    setBasePath(basePath);
  };

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

  const storeIdValues = formState.storeValues === "id";
  const hideInBackgroundField = disabled || basePath.includes(".fields");

  return (
    <Grid>
      {!showChild && (
        <>
          <Cell size={"1-1"}>
            <Grid>
              <Cell size={"1-1"}>
                {disabled ? null : (
                  <>
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
                  </>
                )}
              </Cell>
            </Grid>
          </Cell>

          <Cell size={"1-1"}>
            <Grid style={{ marginTop: 24 }}>
              <Cell size={"1-3"}>
                <AutoCompleteMultiSelect
                  values={analyzers}
                  onRemove={removeAnalyzer}
                  onSelect={addAnalyzer}
                  options={options}
                  label={"Analyzers"}
                  disabled={disabled}
                />
              </Cell>
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
            </Grid>
          </Cell>
        </>
      )}

      <Cell size={"1"}>
        {!showChild && (
          <Fieldset legend={"Fields"}>
            <>
              {disabled && isEmpty(fields) ? null : (
                <FieldList
                  fields={fields}
                  disabled={disabled}
                  dispatch={
                    (dispatch as unknown) as Dispatch<
                      DispatchArgs<LinkProperties>
                    >
                  }
                  basePath={basePath}
                  viewField={handleShowField}
                />
              )}
            </>
          </Fieldset>
        )}

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

        {showChild && fieldName !== undefined && (
          <FieldView
            view={view}
            fields={fields}
            disabled={disabled}
            dispatch={dispatch}
            basePath={childBasePath}
            viewField={handleShowField}
            fieldName={fieldName}
            link={childBasePath.split("[")[1].replace("]", "")}
          />
        )}
      </Cell>
    </Grid>
  );
};

export default LinkPropertiesInput;
