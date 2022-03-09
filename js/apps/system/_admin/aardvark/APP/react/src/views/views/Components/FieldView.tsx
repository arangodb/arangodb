import React, { Dispatch } from "react";
import ViewLayout from "./ViewLayout";
import {
  ArangoTable,
  ArangoTD,
  ArangoTH
} from "../../../components/arango/table";
import Badge from "../../../components/arango/badges";
import { IconButton } from "../../../components/arango/buttons";
import { DispatchArgs } from "../../../utils/constants";
import { LinkProperties } from "../constants";
import { map } from "lodash";
import LinkPropertiesInput from "../forms/inputs/LinkPropertiesInput";
type FieldViewPprops = {
  fields: {};
  disabled: boolean | undefined;
  dispatch: Dispatch<DispatchArgs<LinkProperties>>;
  basePath: string;
  viewField: any;
  link?: string;
  field?: string;
  view?: string;
};

const FieldView: React.FC<FieldViewPprops> = ({
  fields,
  disabled,
  dispatch,
  basePath,
  viewField,
  field
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

  const loopFields = (fields: any) => {
    for (const f in fields) {
      if (field === f) {
        const toReturn = fields[f];
        return toReturn;
      }
    }
  };

  const newField = loopFields(fields);

  return (
    <ViewLayout disabled={disabled} field={field}>
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
          <tr key={field} style={{ borderBottom: "1px  solid #DDD" }}>
            <ArangoTD seq={disabled ? 0 : 1}>{field}</ArangoTD>
            <ArangoTD seq={disabled ? 1 : 2}>
              {newField &&
                map(newField.analyzers, a => <Badge key={a} name={a} />)}
              <LinkPropertiesInput
                formState={newField}
                disabled={disabled}
                basePath={`${basePath}.fields[${field}]`}
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
                  onClick={getFieldRemover(newField)}
                />
                <IconButton
                  icon={"eye"}
                  type={"warning"}
                  onClick={() => viewField(newField)}
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
