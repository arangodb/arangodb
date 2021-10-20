import { DispatchArgs, FormProps } from "../../../utils/constants";
import { FormState, LinkProperties } from "../constants";
import React, { ChangeEvent, Dispatch, useEffect } from "react";
import { ArangoTable, ArangoTD, ArangoTH } from "../../../components/arango/table";
import { isEmpty, map, negate, pickBy } from 'lodash';
import Checkbox from "../../../components/pure-css/form/Checkbox";
import LinkPropertiesInput from "./inputs/LinkPropertiesInput";
import { IconButton } from "../../../components/arango/buttons";
import Textbox from "../../../components/pure-css/form/Textbox";
import { useLinkState } from "../helpers";

type LinkPropertiesFormProps = FormProps<FormState> & {
  cache: { [key: string]: any };
};

const LinkPropertiesForm = ({ formState, dispatch, disabled, cache }: LinkPropertiesFormProps) => {
  const [collection, setCollection, addDisabled, links] = useLinkState(formState, 'links');

  useEffect(() => {
    cache.links = cache.links || {};

    Object.assign(cache.links, pickBy(links, negate(isEmpty)));
  }, [cache, links]);

  const updateCollection = (event: ChangeEvent<HTMLSelectElement>) => {
    setCollection(event.target.value);
  };

  const toggleLink = (collection: string, checked: boolean) => {
    dispatch({
      type: 'setField',
      field: {
        path: `links[${collection}]`,
        value: checked ? null : cache.links[collection]
      }
    });
  };

  const getLinkToggler = (collection: string) => (event: ChangeEvent<HTMLInputElement>) => {
    toggleLink(collection, event.target.checked);
  };

  const addLink = () => {
    dispatch({
      type: 'setField',
      field: {
        path: `links[${collection}]`,
        value: {}
      }
    });
    setCollection('');
    cache.links[collection] = {};
  };

  return <ArangoTable>
    <thead>
    <tr>
      {
        disabled
          ? null
          : <ArangoTH seq={0} style={{ width: '2%' }}><i className={'fa fa-trash-o'}/></ArangoTH>
      }
      <ArangoTH seq={disabled ? 0 : 1} style={{ width: '8%' }}>Collection</ArangoTH>
      <ArangoTH seq={disabled ? 1 : 2} style={{ width: '90%' }}>Properties</ArangoTH>
    </tr>
    </thead>
    <tbody>
    {
      map(links, (properties, coll) => {
        return <tr key={coll} style={{ borderBottom: '1px  solid #DDD' }}>
          {
            disabled
              ? null
              : <ArangoTD seq={0} valign={'middle'}><Checkbox onChange={getLinkToggler(coll)}
                                                              checked={!links[coll]}/></ArangoTD>
          }
          <ArangoTD seq={disabled ? 0 : 1}>{coll}</ArangoTD>
          <ArangoTD seq={disabled ? 1 : 2}>
            <LinkPropertiesInput formState={properties || cache.links[coll]} disabled={!properties}
                                 dispatch={dispatch as unknown as Dispatch<DispatchArgs<LinkProperties>>}
                                 basePath={`links[${coll}]`}/>
          </ArangoTD>
        </tr>;
      })
    }
    {
      disabled
        ? null
        : <tr style={{ borderBottom: '1px  solid #DDD' }}>
          <ArangoTD seq={0} colSpan={2}>
            <Textbox type={'text'} placeholder={'Collection'} onChange={updateCollection} value={collection}/>
          </ArangoTD>
          <ArangoTD seq={1}>
            <IconButton icon={'plus'} type={'warning'} onClick={addLink} disabled={addDisabled}>Add</IconButton>
          </ArangoTD>
        </tr>
    }
    </tbody>
  </ArangoTable>;
};

export default LinkPropertiesForm;
