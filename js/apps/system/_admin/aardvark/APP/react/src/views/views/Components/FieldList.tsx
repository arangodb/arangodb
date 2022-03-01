import React, { Dispatch } from "react";
import { DispatchArgs } from "../../../utils/constants";
import { LinkProperties } from "../constants";
import { map } from "lodash";
import {
  ArangoTable,
  ArangoTD,
  ArangoTH
} from "../../../components/arango/table";
import Badge from "../../../components/arango/badges";
import { IconButton } from "../../../components/arango/buttons";
import AutoCompleteMultiSelect from "../../../components/pure-css/form/AutoCompleteMultiSelect";

type FieldViewProps = {
  fields: any;
  disabled: boolean | undefined;
  dispatch: Dispatch<DispatchArgs<LinkProperties>>;
  basePath: string;
  options?: string[] | number[] | undefined;
  label?: React.ReactNode;
  onSelect: (value: string | number) => void;
  values?: string[] | number[];
  onRemove: (value: string | number) => void;
};

const FieldView: React.FC<FieldViewProps> = ({
  fields,
  disabled,
  dispatch,
  basePath,
  options,
  label,
  onRemove,
  onSelect
}) => {
  const removeField = (field: string | number) => {
    dispatch({
      type: "unsetField",
      field: {
        path: `fields[${field}]`
      },
      basePath
    });
  };

  const getFieldRemover = (field: string | number) => () => {
    removeField(field);
  };

  return (
    <>
      <ArangoTable style={{ marginLeft: 0 }}>
        <thead>
          <tr>
            <ArangoTH seq={disabled ? 0 : 1} style={{ width: "8%" }}>
              Field
            </ArangoTH>
            <ArangoTH seq={disabled ? 1 : 2} style={{ width: "72%" }}>
              Analyzers
            </ArangoTH>
            {disabled ? null : (
              <ArangoTH seq={0} style={{ width: "20%" }}>
                Action
              </ArangoTH>
            )}
          </tr>
        </thead>
        <tbody>
          {map(fields, (properties, fld) => (
            <tr key={fld} style={{ borderBottom: "1px  solid #DDD" }}>
              <ArangoTD seq={disabled ? 0 : 1}>{fld}</ArangoTD>
              <ArangoTD seq={disabled ? 1 : 2}>
                {properties &&
                  map(properties.analyzers, a => <Badge name={a} />)}
                <AutoCompleteMultiSelect
                  values={properties.analyzers}
                  options={options}
                  onRemove={onRemove}
                  onSelect={onSelect}
                  label={label}
                />

                {/* <LinkPropertiesInput
              formState={properties}
              disabled={disabled}
              basePath={`${basePath}.fields[${fld}]`}
              dispatch={
                (dispatch as unknown) as Dispatch<DispatchArgs<LinkProperties>>
              }
            /> */}
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
          ))}
        </tbody>
      </ArangoTable>
    </>
  );
};

export default FieldView;
