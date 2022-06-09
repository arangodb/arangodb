import React, { useContext } from "react";
import ViewLinkLayout from "./ViewLinkLayout";
import { ArangoTable, ArangoTD } from "../../../components/arango/table";
import { ViewContext } from "../constants";
import LinkPropertiesInput from "../forms/inputs/LinkPropertiesInput";
import { useRouteMatch } from "react-router-dom";
import { get, last } from "lodash";

type FieldViewProps = {
  disabled: boolean | undefined;
};

const FieldView = ({ disabled }: FieldViewProps) => {
  const { formState, dispatch } = useContext(ViewContext);
  const match = useRouteMatch();

  const fieldStr = get(match.params, 'field');
  const fieldName = last(fieldStr.split('/')) as string;

  const fragments = match.url.slice(1).split('/');
  let basePath = `links[${fragments[0]}]`;
  for (let i = 1; i < fragments.length - 1; ++i) {
    basePath += `.fields[${fragments[i]}]`;
  }

  const field = get(formState, `${basePath}.fields[${fieldName}]`);

  return <ViewLinkLayout fragments={fragments}>
      <ArangoTable style={{ marginLeft: 0 }}>
        <tbody>
        <tr key={fieldName} style={{ borderBottom: "1px  solid #794242" }}>
          <ArangoTD seq={0}>{fieldName}</ArangoTD>
          <ArangoTD seq={1}>
            {
              field
                ? <LinkPropertiesInput
                  formState={field}
                  disabled={disabled}
                  basePath={`${basePath}.fields[${fieldName}]`}
                  dispatch={dispatch}
                />
                : null
            }
          </ArangoTD>
        </tr>
        </tbody>
      </ArangoTable>
    </ViewLinkLayout>;
};

export default FieldView;
