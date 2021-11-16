import { DispatchArgs, FormProps } from "../../../utils/constants";
import { FormState, LinkProperties } from "../constants";
import React, { ChangeEvent, Dispatch, useEffect, useState } from "react";
import { ArangoTable, ArangoTD, ArangoTH } from "../../../components/arango/table";
import { isEmpty, map, negate, pickBy, without } from 'lodash';
import Checkbox from "../../../components/pure-css/form/Checkbox";
import LinkPropertiesInput from "./inputs/LinkPropertiesInput";
import { IconButton } from "../../../components/arango/buttons";
import { useLinkState } from "../helpers";
import AutoCompleteTextInput from "../../../components/pure-css/form/AutoCompleteTextInput";
import useSWR from 'swr';
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";

type LinkPropertiesFormProps = FormProps<FormState> & {
  cache?: { [key: string]: any };
};

const LinkPropertiesForm = ({ formState, dispatch, disabled, cache = {} }: LinkPropertiesFormProps) => {
  const [collection, setCollection, addDisabled, links] = useLinkState(formState, 'links');
  const { data } = useSWR(['/collection', 'excludeSystem=true'], (path, qs) => getApiRouteForCurrentDB().get(path, qs));
  const [options, setOptions] = useState<string[]>([]);

  useEffect(() => {
    cache.links = cache.links || {};

    Object.assign(cache.links, pickBy(links, negate(isEmpty)));
  }, [cache, links]);

  useEffect(() => {
    if (data) {
      setOptions(map(data.body.result, 'name'));
    }
  }, [data]);

  const updateCollection = (value: string | number) => {
    setCollection(value);
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
    setOptions(without(options, collection));
  };

  return disabled && isEmpty(links)
    ? <span>No links found.</span>
    : <ArangoTable>
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
                : <ArangoTD seq={0} valign={'middle'}>
                  <Checkbox onChange={getLinkToggler(coll)} checked={!links[coll]}/>
                </ArangoTD>
            }
            <ArangoTD seq={disabled ? 0 : 1}>{coll}</ArangoTD>
            <ArangoTD seq={disabled ? 1 : 2}>
              <LinkPropertiesInput formState={properties || cache.links[coll]}
                                   disabled={disabled || !properties}
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
              <AutoCompleteTextInput placeholder={'Collection'} value={collection} minChars={1} spacer={''}
                                     onSelect={updateCollection} matchAny={true} options={options}
                                     onChange={updateCollection} offsetY={280} offsetX={-20}/>
              {/* <Textbox type={'text'} placeholder={'Collection'} onChange={updateCollection} value={collection}/>*/}
            </ArangoTD>
            <ArangoTD seq={1}>
              <IconButton icon={'plus'} type={'warning'} onClick={addLink}
                          disabled={addDisabled}>Add</IconButton>
            </ArangoTD>
          </tr>
      }
      </tbody>
    </ArangoTable>;
};

export default LinkPropertiesForm;
