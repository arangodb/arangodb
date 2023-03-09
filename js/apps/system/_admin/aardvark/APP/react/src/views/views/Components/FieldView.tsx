import React, { useContext, useEffect, useState } from "react";
import ViewLinkLayout from "./ViewLinkLayout";
import { ArangoTable, ArangoTD } from "../../../components/arango/table";
import { ViewContext } from "../constants";
import LinkPropertiesInput from "../forms/inputs/LinkPropertiesInput";
import { useHistory, useLocation, useRouteMatch } from "react-router-dom";
import { get, last } from "lodash";
import { escapeFieldDot } from "../../../utils/fieldHelpers";

type FieldViewProps = {
  disabled: boolean | undefined;
  name: string;
};

const FieldView = ({ disabled }: FieldViewProps) => {
  const [field, setField] = useState<any>();
  const [fieldName, setFieldName] = useState<string | number | undefined>();
  const [basePath, setBasePath] = useState('');
  const [fragments, setFragments] = useState(['']);
  const { formState, dispatch } = useContext(ViewContext);
  const match = useRouteMatch();
  const history = useHistory();
  const location = useLocation();
  useEffect(()=> {
    const fragments = match.url.slice(1).split('/');
    const fieldName = last(fragments);
  
    let basePath = `links[${fragments[0]}]`;
    for (let i = 1; i < fragments.length - 1; ++i) {
      const fieldFragment = escapeFieldDot(fragments[i]);
      basePath += `.fields[${fieldFragment}]`;
    }
    const newFieldName = escapeFieldDot(fieldName);
    const field = get(formState, `${basePath}.fields[${newFieldName}]`);
    setField(field);
    setFragments(fragments);
    setFieldName(newFieldName);
    setBasePath(basePath);

    if (!(Object.keys(formState).length === 1 && (formState as any).name)) {
      if (!field) {
        const newUrl = `/view/${(formState as any).name}`
        if (newUrl !== history.location.pathname) {
          history.replace('/');
        }
      }
    }
  }, [formState, match.url, history, location])
  return <ViewLinkLayout fragments={fragments}>
    <ArangoTable style={{
      marginLeft: 0,
      border: 'none'
    }}>
      <tbody>
      <tr key={fieldName}>
        <ArangoTD seq={0} style={{ paddingLeft: 0 }}>
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
