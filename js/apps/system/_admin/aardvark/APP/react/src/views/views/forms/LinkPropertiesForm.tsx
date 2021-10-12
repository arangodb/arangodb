import { DispatchArgs, FormProps } from "../../../utils/constants";
import { FormState, LinkProperties } from "../constants";
import React, { ChangeEvent, Dispatch, useEffect, useMemo, useState } from "react";
import { ArangoTable, ArangoTD, ArangoTH } from "../../../components/arango/table";
import { map } from 'lodash';
import { getPath } from "../../../utils/helpers";
import Checkbox from "../../../components/pure-css/form/Checkbox";
import LinkPropertiesInput from "./inputs/LinkPropertiesInput";
import { IconButton } from "../../../components/arango/buttons";
import Textbox from "../../../components/pure-css/form/Textbox";

const LinkPropertiesForm = ({ formState, dispatch, disabled }: FormProps<FormState>) => {
  const [collection, setCollection] = useState('');
  const [addDisabled, setAddDisabled] = useState(true);
  const links = useMemo(() => formState.links || {}, [formState.links]);

  useEffect(() => {
    setAddDisabled(Object.keys(links).includes(collection));
  }, [collection, links]);

  const updateCollection = (event: ChangeEvent<HTMLSelectElement>) => {
    setCollection(event.target.value);
  };

  const removeLink = (collection: string) => {
    dispatch({
      type: 'setField',
      field: {
        path: `links${collection}`,
        value: null
      }
    });
  };

  const getRemover = (collection: string) => () => {
    removeLink(collection);
  };

  const addLink = (collection: string) => {
    dispatch({
      type: 'setField',
      field: {
        path: `links[${collection}]`,
        value: {}
      }
    });
  };

  const getWrappedDispatch = (collection: string): Dispatch<DispatchArgs<LinkProperties>> => (action: DispatchArgs<LinkProperties>) => {
    action.basePath = getPath(`links[${collection}]`, action.basePath);
    (dispatch as unknown as Dispatch<DispatchArgs<LinkProperties>>)(action);
  };

  return <ArangoTable>
    <thead>
    <tr>
      {
        disabled
          ? null
          : <ArangoTH seq={0} style={{ width: '5%' }}><i className={'fa fa-trash-o'}/></ArangoTH>
      }
      <ArangoTH seq={disabled ? 0 : 1} style={{ width: '10%' }}>Collection</ArangoTH>
      <ArangoTH seq={disabled ? 1 : 2} style={{ width: '85%' }}>Properties</ArangoTH>
    </tr>
    </thead>
    <tbody>
    {
      map(links, (link, coll) => {
        const linkDispatch = getWrappedDispatch(coll);

        return <tr key={coll} style={{ borderBottom: '1px  solid #DDD' }}>
          {
            disabled
              ? null
              : <ArangoTD seq={0} valign={'middle'}><Checkbox onChange={getRemover(coll)}/></ArangoTD>
          }
          <ArangoTD seq={disabled ? 0 : 1}>{coll}</ArangoTD>
          <ArangoTD seq={disabled ? 1 : 2}>
            {
              link
                ? <LinkPropertiesInput formState={link} dispatch={linkDispatch} basePath={`links[${coll}]`}/>
                : null
            }
          </ArangoTD>
        </tr>;
      })
    }
    {
      disabled
        ? null
        : <tr style={{ borderBottom: '1px  solid #DDD' }}>
          <ArangoTD seq={0} colSpan={3}>
            <Textbox type={'text'} placeholder={'Collection'} onChange={updateCollection}/>&nbsp;
            <IconButton icon={'plus'} type={'warning'} onClick={addLink} disabled={addDisabled}>Add</IconButton>
          </ArangoTD>
        </tr>
    }
    </tbody>
  </ArangoTable>;
};

export default LinkPropertiesForm;
