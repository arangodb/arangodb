import React, { Dispatch } from "react";
import ViewLayout from "./ViewLayout";
import { ArangoTable, ArangoTD } from "../../../components/arango/table";
// import Badge from "../../../components/arango/badges";
import { IconButton } from "../../../components/arango/buttons";
import { DispatchArgs } from "../../../utils/constants";
import { LinkProperties } from "../constants";
// import { map } from "lodash";
import LinkPropertiesInput from "../forms/inputs/LinkPropertiesInput";
type FieldViewPprops = {
  fields: {};
  disabled: boolean | undefined;
  dispatch: Dispatch<DispatchArgs<LinkProperties>>;
  basePath: string;
  viewField: any;
  link?: string;
  fieldName?: string;
  view?: string;
};

const FieldView: React.FC<FieldViewPprops> = ({
  fields,
  disabled,
  dispatch,
  basePath,
  viewField,
  fieldName,
  view,
  link
}) => {
  const path = `${basePath}.fields[${fieldName}]`;
  console.log(path);
  const removeField = (field: string | undefined) => {
    dispatch({
      type: "unsetField",
      field: {
        path: `fields[${field}]`
      },
      basePath
    });
  };

  const getFieldRemover = (field: string | undefined) => () => {
    removeField(field);
  };

  const loopFields = (fields: any) => {
    for (const f in fields) {
      if (fieldName === f) {
        const toReturn = fields[f];
        return toReturn;
      }
    }
  };

  const newField = loopFields(fields);

  return (
    <ViewLayout disabled={disabled} field={fieldName} view={view} link={link}>
      <ArangoTable style={{ marginLeft: 0 }}>
        <tbody>
          <tr key={fieldName} style={{ borderBottom: "1px  solid #DDD" }}>
            <ArangoTD seq={disabled ? 0 : 1}>{fieldName}</ArangoTD>
            <ArangoTD seq={disabled ? 1 : 2}>
              {/* {newField &&
                 map(newField.analyzers, a => <Badge key={a} name={a} />)} */}
              <LinkPropertiesInput
                formState={newField}
                disabled={disabled}
                basePath={`${basePath}.fields[${fieldName}].fields[${fieldName}]`}
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
                  onClick={getFieldRemover(fieldName)}
                />
                <IconButton
                  icon={"eye"}
                  type={"warning"}
                  onClick={() => viewField(fieldName)}
                />
              </ArangoTD>
            )}
          </tr>
        </tbody>
      </ArangoTable>
    </ViewLayout>
  );
};

export default FieldView;
