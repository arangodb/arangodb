import React, { useContext } from "react";
import ViewLinkLayout from "./ViewLinkLayout";
import { ArangoTable, ArangoTD } from "../../../components/arango/table";
import { ViewContext } from "../constants";
import LinkPropertiesInput from "../forms/inputs/LinkPropertiesInput";
import { useRouteMatch } from "react-router-dom";
import { get, last } from "lodash";
import { escapeFieldDot } from "../../../utils/fieldHelpers";

type FieldViewProps = {
  disabled: boolean | undefined;
  name: string;
};

const FieldView = ({ disabled }: FieldViewProps) => {
  const { formState, dispatch } = useContext(ViewContext);
  const match = useRouteMatch();

  const fragments = match.url.slice(1).split('/');
  const fieldName = last(fragments);

  let basePath = `links[${fragments[0]}]`;
  for (let i = 1; i < fragments.length - 1; ++i) {
    const fieldFragment = escapeFieldDot(fragments[i]);
    basePath += `.fields[${fieldFragment}]`;
  }
  const newFieldName = escapeFieldDot(fieldName);

  const field = get(formState, `${basePath}.fields[${newFieldName}]`);
  return <ViewLinkLayout fragments={fragments}>
    <ArangoTable style={{
      marginLeft: 0,
      border: 'none'
    }}>
      <tbody>
      <tr key={newFieldName}>
        <ArangoTD seq={0} style={{ paddingLeft: 0 }}>
          {
            field
              ? <LinkPropertiesInput
                formState={field}
                disabled={disabled}
                basePath={`${basePath}.fields[${newFieldName}]`}
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
