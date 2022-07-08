import React, { useContext } from "react";
import { map } from "lodash";
import {
  ArangoTable,
  ArangoTD,
  ArangoTH
} from "../../../components/arango/table";
import Badge from "../../../components/arango/badges";
import { IconButton } from "../../../components/arango/buttons";
import { ViewContext } from "../ViewLinksReactView";

type FieldListProps = {
  fields: object | any;
  disabled: boolean | undefined;
  basePath: string;
  viewField: any;
};

const FieldView = ({
  fields,
  disabled,
  basePath,
  viewField
}: FieldListProps) => {
  const { dispatch } = useContext(ViewContext);

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
                map(properties.analyzers, a => <Badge key={a} name={a} />)}
              {/* {showField && (
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
              )} */}
            </ArangoTD>
            {disabled ? null : (
              <ArangoTD seq={0} valign={"middle"}>
                <IconButton
                  icon={"trash-o"}
                  type={"danger"}
                  onClick={getFieldRemover(fld)}
                />
                <IconButton
                  icon={"edit"}
                  type={"warning"}
                  onClick={() => viewField(fld, properties)}
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
