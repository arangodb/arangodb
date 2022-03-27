import React, { Dispatch, useContext } from "react";
import ViewLayout from "./ViewLayout";
import { ArangoTable, ArangoTD } from "../../../components/arango/table";
import { IconButton } from "../../../components/arango/buttons";
import { DispatchArgs } from "../../../utils/constants";
import { LinkProperties } from "../constants";
import LinkPropertiesInput from "../forms/inputs/LinkPropertiesInput";
import { ViewContext } from "../ViewLinksReactView";
import { get } from "lodash";

type FieldViewProps = {
  disabled: boolean | undefined;
  basePath: string;
  viewField: any;
  link?: string;
  fieldName?: string;
  view?: string;
};

const FieldView = ({
  disabled,
  basePath,
  fieldName,
  view,
  link
}: FieldViewProps) => {
  const { formState, dispatch } = useContext(ViewContext);

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

  const field = get(formState, `${basePath}.fields[${fieldName}]`);

  return (
    <ViewLayout field={fieldName} view={view} link={link}>
      <ArangoTable style={{ marginLeft: 0 }}>
        <tbody>
          <tr key={fieldName} style={{ borderBottom: "1px  solid #794242" }}>
            <ArangoTD seq={disabled ? 0 : 1}>{fieldName}</ArangoTD>
            <ArangoTD seq={disabled ? 1 : 2}>
              {/* {newField &&
                 map(newField.analyzers, a => <Badge key={a} name={a} />)} */}
              <LinkPropertiesInput
                formState={field}
                disabled={disabled}
                basePath={`${basePath}.fields[${fieldName}]`}
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
              </ArangoTD>
            )}
          </tr>
        </tbody>
      </ArangoTable>
    </ViewLayout>
  );
};

export default FieldView;
