import { DispatchArgs, FormProps } from "../../../../utils/constants";
import { LinkProperties } from "../../constants";
import React, { ChangeEvent, Dispatch, useEffect, useState } from "react";
import { isEmpty, map, without } from "lodash";
import { Cell, Grid } from "../../../../components/pure-css/grid";
import Checkbox from "../../../../components/pure-css/form/Checkbox";
import { ArangoTable, ArangoTD, ArangoTH } from "../../../../components/arango/table";
import Textbox from "../../../../components/pure-css/form/Textbox";
import { IconButton } from "../../../../components/arango/buttons";
import { useLinkState } from "../../helpers";
import Fieldset from "../../../../components/pure-css/form/Fieldset";
import { getBooleanFieldSetter } from "../../../../utils/helpers";
import AutoCompleteMultiSelect from "../../../../components/pure-css/form/AutoCompleteMultiSelect";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../../../utils/arangoClient";

type LinkPropertiesInputProps = FormProps<LinkProperties> & {
  basePath: string;
};

const LinkPropertiesInput = ({ formState, dispatch, disabled, basePath }: LinkPropertiesInputProps) => {
  const [field, setField, addDisabled, fields] = useLinkState(formState, 'fields');
  const { data } = useSWR('/analyzer', (path) => getApiRouteForCurrentDB().get(path));
  const [options, setOptions] = useState<string[]>([]);

  useEffect(() => {
    if (data) {
      setOptions(map(data.body.result, 'name').sort());
    }
  }, [data]);

  const updateField = (event: ChangeEvent<HTMLSelectElement>) => {
    setField(event.target.value);
  };

  const removeField = (field: string) => {
    dispatch({
      type: 'unsetField',
      field: {
        path: `fields[${field}]`
      },
      basePath
    });
  };

  const getFieldRemover = (field: string) => () => {
    removeField(field);
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
  };

  // const updateAnalyzers = (event: ChangeEvent<HTMLTextAreaElement>) => {
  //   const analyzers = event.target.value.split('\n');
  //
  //   if (filter(analyzers, negate(isEmpty)).length) {
  //     dispatch({
  //       type: 'setField',
  //       field: {
  //         path: 'analyzers',
  //         value: analyzers
  //       },
  //       basePath
  //     });
  //   } else {
  //     dispatch({
  //       type: 'unsetField',
  //       field: {
  //         path: 'analyzers'
  //       },
  //       basePath
  //     });
  //   }
  // };

  // const getAnalyzers = () => {
  //   return (formState.analyzers || []).join('\n');
  // };

  const analyzers = formState.analyzers || [];

  const addAnalyzer = (analyzer: string | number) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'analyzers',
        value: analyzers.concat([analyzer as string])
      },
      basePath
    });
    setOptions(without(options, analyzer as string));
  };

  const removeAnalyzer = (analyzer: string | number) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'analyzers',
        value: without(analyzers, analyzer)
      },
      basePath
    });
    setOptions(options.concat([analyzer as string]).sort());
  };

  const updateStoreValues = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch(
      {
        type: 'setField',
        field: {
          path: 'storeValues',
          value: event.target.checked ? 'id' : 'none'
        },
        basePath
      }
    );
  };

  const storeIdValues = formState.storeValues === 'id';

  return <Grid>
    <Cell size={'1-3'}>
      {/* <Textarea value={getAnalyzers()} onChange={updateAnalyzers} disabled={disabled} rows={4}*/}
      {/*          label={'Analyzers (One per line)'}/>*/}
      <AutoCompleteMultiSelect values={analyzers} onRemove={removeAnalyzer} onSelect={addAnalyzer}
                               options={options} label={'Analyzers'} disabled={disabled}/>
    </Cell>
    <Cell size={'1-3'}>
      <Grid style={{ marginTop: 24 }}>
        <Cell size={'1'}>
          <Checkbox onChange={getBooleanFieldSetter('includeAllFields', dispatch, basePath)} inline={true}
                    label={"Include All Fields"} disabled={disabled} checked={formState.includeAllFields}/>
        </Cell>
        <Cell size={'1'}>
          <Checkbox onChange={getBooleanFieldSetter('trackListPositions', dispatch, basePath)}
                    label={'Track List Positions'} disabled={disabled} inline={true}
                    checked={formState.trackListPositions}/>
        </Cell>
        <Cell size={'1'}>
          <Checkbox onChange={updateStoreValues} label={'Store ID Values'} disabled={disabled} inline={true}
                    checked={storeIdValues}/>
        </Cell>
      </Grid>
    </Cell>
    {
      disabled
        ? null
        : <Cell size={'1-3'} style={{ marginTop: 24 }}>
          <Checkbox onChange={getBooleanFieldSetter('inBackground', dispatch, basePath)}
                    label={'In Background'} inline={true}
                    disabled={disabled} checked={formState.inBackground}/>
        </Cell>
    }
    {
      disabled && isEmpty(fields)
        ? null
        : <Cell size={'1'}>
          <Fieldset legend={'Fields'}>
            <ArangoTable style={{ marginLeft: 0 }}>
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
                        : <ArangoTD seq={0} valign={'middle'}>
                          <IconButton icon={'trash-o'} type={'danger'} onClick={getFieldRemover(fld)}/>
                        </ArangoTD>
                    }
                    <ArangoTD seq={disabled ? 0 : 1}>{fld}</ArangoTD>
                    <ArangoTD seq={disabled ? 1 : 2}>
                      <LinkPropertiesInput formState={properties} disabled={disabled}
                                           basePath={`${basePath}.fields[${fld}]`}
                                           dispatch={dispatch as unknown as Dispatch<DispatchArgs<LinkProperties>>}/>
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
                      <IconButton icon={'plus'} type={'warning'} onClick={addField} disabled={addDisabled}>
                        Add
                      </IconButton>
                    </ArangoTD>
                  </tr>
              }
              </tbody>
            </ArangoTable>
          </Fieldset>
        </Cell>
    }
  </Grid>;
};

export default LinkPropertiesInput;
