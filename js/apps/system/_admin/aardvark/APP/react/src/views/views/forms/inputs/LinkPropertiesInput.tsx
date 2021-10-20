import { DispatchArgs, FormProps } from "../../../../utils/constants";
import { LinkProperties } from "../../constants";
import React, { ChangeEvent, Dispatch, useEffect, useMemo, useState } from "react";
import { filter, get, has, isEmpty, map, negate, pickBy, set } from "lodash";
import { Cell, Grid } from "../../../../components/pure-css/grid";
import Textarea from "../../../../components/pure-css/form/Textarea";
import Checkbox from "../../../../components/pure-css/form/Checkbox";
import Select from "../../../../components/pure-css/form/Select";
import { ArangoTable, ArangoTD, ArangoTH } from "../../../../components/arango/table";
import Textbox from "../../../../components/pure-css/form/Textbox";
import { IconButton } from "../../../../components/arango/buttons";

type LinkPropertiesInputProps = FormProps<LinkProperties> & {
  basePath: string;
  cache: { [key: string]: any };
};

const LinkPropertiesInput = ({
                               formState,
                               dispatch,
                               disabled,
                               basePath,
                               cache
                             }: LinkPropertiesInputProps) => {
  const [field, setField] = useState('');
  const [addDisabled, setAddDisabled] = useState(true);
  const fields = useMemo(() => (formState.fields || {}), [formState.fields]);
  const fieldsPath = useMemo(() => `${basePath}.fields`, [basePath]);
  const fieldsCache = useMemo(() => get(cache, `${basePath}.fields`, {}), [basePath, cache]);

  useEffect(() => {
    setAddDisabled(!field || Object.keys(fields).includes(field));
  }, [field, fields]);

  useEffect(() => {
    if (!has(cache, fieldsPath)) {
      set(cache, fieldsPath, {});
    }

    Object.assign(fieldsCache, pickBy(fields, negate(isEmpty)));
  }, [cache, fields, fieldsCache, fieldsPath]);

  const updateField = (event: ChangeEvent<HTMLSelectElement>) => {
    setField(event.target.value);
  };

  const toggleField = (field: string, checked: boolean) => {
    dispatch({
      type: 'setField',
      field: {
        path: `fields[${field}]`,
        value: checked ? null : fieldsCache[field]
      },
      basePath
    });
  };

  const getFieldToggler = (field: string) => (event: ChangeEvent<HTMLInputElement>) => {
    toggleField(field, event.target.checked);
  };

  const addField = () => {
    dispatch({
      type: 'setField',
      field: {
        path: `fields[${field}]`,
        value: {}
      },
      basePath
    });
    setField('');
    fieldsCache[field] = {};
  };

  const updateAnalyzers = (event: ChangeEvent<HTMLTextAreaElement>) => {
    const analyzers = event.target.value.split('\n');

    if (filter(analyzers, negate(isEmpty)).length) {
      dispatch({
        type: 'setField',
        field: {
          path: 'analyzers',
          value: analyzers
        },
        basePath
      });
    } else {
      dispatch({
        type: 'unsetField',
        field: {
          path: 'analyzers'
        },
        basePath
      });
    }
  };

  const getAnalyzers = () => {
    return (formState.analyzers || []).join('\n');
  };

  const getBooleanFieldSetter = (field: string) => (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: field,
        value: event.target.checked
      },
      basePath
    });
  };

  const updateStoreValues = (event: ChangeEvent<HTMLSelectElement>) => {
    dispatch(
      {
        type: 'setField',
        field: {
          path: 'storeValues',
          value: event.target.value
        },
        basePath
      }
    );
  };

  console.log(cache);

  return <Grid>
    <Cell size={'1-3'}>
      <Textarea value={getAnalyzers()} onChange={updateAnalyzers} disabled={disabled} label={'Analyzers'}/>
    </Cell>
    <Cell size={'2-3'}>
      <Grid>
        <Cell size={'1-2'}>
          <Checkbox onChange={getBooleanFieldSetter('includeAllFields')} label={'Include All Fields'}
                    disabled={disabled} checked={formState.includeAllFields}/>
        </Cell>
        <Cell size={'1-2'}>
          <Checkbox onChange={getBooleanFieldSetter('trackListPositions')} label={'Track List Positions'}
                    disabled={disabled} checked={formState.trackListPositions}/>
        </Cell>
        <Cell size={'1-2'}>
          <Select value={formState.storeValues || 'none'} onChange={updateStoreValues} disabled={disabled}
                  label={'Store Values'}>
            <option key={'none'} value={'none'}>None</option>
            <option key={'id'} value={'id'}>ID</option>
          </Select>
        </Cell>
        <Cell size={'1-2'}>
          <Checkbox onChange={getBooleanFieldSetter('inBackground')} label={'In Background'}
                    disabled={disabled} checked={formState.inBackground}/>
        </Cell>
      </Grid>
    </Cell>
    <Cell size={'1'}>
      <ArangoTable>
        <thead>
        <tr>
          {
            disabled
              ? null
              : <ArangoTH seq={0} style={{ width: '2%' }}><i className={'fa fa-trash-o'}/></ArangoTH>
          }
          <ArangoTH seq={disabled ? 0 : 1} style={{ width: '8%' }}>Field</ArangoTH>
          <ArangoTH seq={disabled ? 1 : 2} style={{ width: '90%' }}>Properties</ArangoTH>
        </tr>
        </thead>
        <tbody>
        {
          map(fields, (properties, fld) => {
            return <tr key={fld} style={{ borderBottom: '1px  solid #DDD' }}>
              {
                disabled
                  ? null
                  : <ArangoTD seq={0} valign={'middle'}><Checkbox onChange={getFieldToggler(fld)}
                                                                  checked={!fields[fld]}/></ArangoTD>
              }
              <ArangoTD seq={disabled ? 0 : 1}>{fld}</ArangoTD>
              <ArangoTD seq={disabled ? 1 : 2}>
                <LinkPropertiesInput formState={properties || fieldsCache[fld]} disabled={!properties}
                                     dispatch={dispatch as unknown as Dispatch<DispatchArgs<LinkProperties>>}
                                     basePath={`${basePath}.fields[${fld}]`} cache={cache}/>
              </ArangoTD>
            </tr>;
          })
        }
        {
          disabled
            ? null
            : <tr style={{ borderBottom: '1px  solid #DDD' }}>
              <ArangoTD seq={0} colSpan={2}>
                <Textbox type={'text'} placeholder={'Field'} onChange={updateField} value={field}/>
              </ArangoTD>
              <ArangoTD seq={1}>
                <IconButton icon={'plus'} type={'warning'} onClick={addField}
                            disabled={addDisabled}>Add</IconButton>
              </ArangoTD>
            </tr>
        }
        </tbody>
      </ArangoTable>
    </Cell>
  </Grid>;
};

export default LinkPropertiesInput;
