import React, { Dispatch, useState } from "react";
import { DispatchArgs } from "../../../utils/constants";
import { LinkProperties } from "../constants";
import { map } from "lodash";
import LinkPropertiesInput from "../forms/inputs/LinkPropertiesInput";
import {
  ArangoTable,
  ArangoTD,
  ArangoTH
} from "../../../components/arango/table";
import Badge from "../../../components/arango/badges";
import { IconButton } from "../../../components/arango/buttons";

type FieldListProps = {
  fields: any;
  disabled: boolean | undefined;
  dispatch: Dispatch<DispatchArgs<LinkProperties>>;
  basePath: string;
  viewField: (field: string | number) => void;
};

const FieldView: React.FC<FieldListProps> = ({
  fields,
  disabled,
  dispatch,
  basePath
}) => {
  const [showField, setShowField] = useState(false);

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

  const viewFieled = (field: string | number) => {
    console.log(field);
    setShowField(!showField);
  };

  return (
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
              {properties && map(properties.analyzers, a => <Badge name={a} />)}
              {showField && (
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
              )}
            </ArangoTD>
            {disabled ? null : (
              <ArangoTD seq={0} valign={"middle"}>
                <IconButton
                  icon={"trash-o"}
                  type={"danger"}
                  onClick={getFieldRemover(fld)}
                />
                <IconButton
                  icon={!showField ? "edit" : "close"}
                  type={"warning"}
                  onClick={() => viewFieled(fld)}
                />
              </ArangoTD>
            )}
          </tr>
        ))}
      </tbody>
    </ArangoTable>
  );
};

export default FieldView;
