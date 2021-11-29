import { DispatchArgs, FormProps } from "../../../utils/constants";
import { FormState, LinkProperties } from "../constants";
import React, { Dispatch, useEffect, useState } from "react";
import { ArangoTable, ArangoTD, ArangoTH } from "../../../components/arango/table";
import { isEmpty, map, without } from 'lodash';
import LinkPropertiesInput from "./inputs/LinkPropertiesInput";
import { IconButton } from "../../../components/arango/buttons";
import { useLinkState } from "../helpers";
import AutoCompleteTextInput from "../../../components/pure-css/form/AutoCompleteTextInput";
import useSWR from 'swr';
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";

const LinkPropertiesForm = ({ formState, dispatch, disabled }: FormProps<FormState>) => {
  const [collection, setCollection, addDisabled, links] = useLinkState(formState, 'links');
  const { data } = useSWR(['/collection', 'excludeSystem=true'], (path, qs) => getApiRouteForCurrentDB().get(path, qs));
  const [options, setOptions] = useState<string[]>([]);

  useEffect(() => {
    if (data) {
      setOptions(map(data.body.result, 'name').sort());
    }
  }, [data]);

  const updateCollection = (value: string | number) => {
    setCollection(value);
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
    setOptions(without(options, collection));
  };

  const removeLink = (collection: string) => {
    dispatch({
      type: 'setField',
      field: {
        path: `links[${collection}]`,
        value: null
      }
    });
  };

  const getLinkRemover = (collection: string) => () => {
    removeLink(collection);
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
          return properties
            ? <tr key={coll} style={{ borderBottom: '1px  solid #DDD' }}>
              {
                disabled
                  ? null
                  : <ArangoTD seq={0} valign={'middle'}>
                    <IconButton icon={'trash-o'} type={'danger'} onClick={getLinkRemover(coll)}/>
                  </ArangoTD>
              }
              <ArangoTD seq={disabled ? 0 : 1}>{coll}</ArangoTD>
              <ArangoTD seq={disabled ? 1 : 2}>
                <LinkPropertiesInput formState={properties}
                                     disabled={disabled || !properties}
                                     dispatch={dispatch as unknown as Dispatch<DispatchArgs<LinkProperties>>}
                                     basePath={`links[${coll}]`}/>
              </ArangoTD>
            </tr>
            : null;
        })
      }
      {
        disabled
          ? null
          : <tr style={{ borderBottom: '1px  solid #DDD' }}>
            <ArangoTD seq={0} colSpan={2}>
              <AutoCompleteTextInput placeholder={'Collection'} value={collection} minChars={1} spacer={''}
                                     onSelect={updateCollection} matchAny={true} options={options}
                                     onChange={updateCollection}/>
            </ArangoTD>
            <ArangoTD seq={1}>
              <IconButton icon={'plus'} type={'warning'} onClick={addLink}
                          disabled={addDisabled || !options.includes(collection)}>Add</IconButton>
            </ArangoTD>
          </tr>
      }
      </tbody>
    </ArangoTable>;
};

export default LinkPropertiesForm;
