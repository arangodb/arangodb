import React, { useContext } from "react";
import ViewLinkLayout from "./ViewLinkLayout";
import { ArangoTable, ArangoTD } from "../../../components/arango/table";
import { FormState, ViewContext } from "../constants";
import LinkPropertiesInput from "../forms/inputs/LinkPropertiesInput";
import { useRouteMatch } from "react-router-dom";
import { get, last } from "lodash";
import { SaveButton } from "../Actions";

type FieldViewProps = {
  disabled: boolean | undefined;
  name: string;
};

const FieldView = ({ disabled, name }: FieldViewProps) => {
  const { formState, dispatch, isAdminUser, changed, setChanged } = useContext(ViewContext);
  const match = useRouteMatch();

  const fragments = match.url.slice(1).split('/');
  const fieldName = last(fragments);

  let basePath = `links[${fragments[0]}]`;
  for (let i = 1; i < fragments.length - 1; ++i) {
    basePath += `.fields[${fragments[i]}]`;
  }

  const field = get(formState, `${basePath}.fields[${fieldName}]`);

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
    {
      isAdminUser && changed
        ? <div className="tab-pane tab-pane-modal active" id="Save">
          <SaveButton view={formState as FormState} setChanged={setChanged} oldName={name}/>
        </div>
        : null
    }
  </ViewLinkLayout>;
};

export default FieldView;
