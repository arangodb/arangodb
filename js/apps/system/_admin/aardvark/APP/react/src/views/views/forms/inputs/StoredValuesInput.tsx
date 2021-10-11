import { DispatchArgs, FormProps } from "../../../../utils/constants";
import { getPath } from "../../../../utils/helpers";
import { StoredValue, StoredValues } from "../../constants";
import Fieldset from "../../../../components/pure-css/form/Fieldset";
import { ArangoTable, ArangoTD, ArangoTH } from "../../../../components/arango/table";
import React, { ChangeEvent, Dispatch } from "react";
import Select from "../../../../components/pure-css/form/Select";
import styled from "styled-components";
import Textbox from "../../../../components/pure-css/form/Textbox";

const StyledButtonSmall = styled.button`
  &&& {
    margin-left: 0;
    font-size: 70%;
    padding: 4px 10px;
  }
`;
const StyledIcon = styled.i`
  &&& {
    margin-left: auto;
  }
`;

const columnWidths: { [key: string]: number[] } = {
  'false': [5, 75, 10, 10],
  'true': [5, 85, 10, 0]
};

const FieldsInput = ({ formState, dispatch, disabled }: FormProps<StoredValue>) => {
  const items = formState.fields;

  const removeField = (index: number) => {
    const tempItems = items.slice();

    tempItems.splice(index, 1);
    dispatch({
      type: 'setField',
      field: {
        path: 'fields',
        value: tempItems
      }
    });
  };

  const getRemover = (index: number) => () => {
    removeField(index);
  };

  const moveUp = (index: number) => {
    const tempItems = items.slice();
    const item = tempItems[index];
    const prevItem = tempItems[index - 1];

    tempItems.splice(index - 1, 2, item, prevItem);
    dispatch({
      type: 'setField',
      field: {
        path: 'fields',
        value: tempItems
      }
    });
  };

  const moveDown = (index: number) => {
    const tempItems = items.slice();
    const item = tempItems[index];
    const nextItem = tempItems[index + 1];

    tempItems.splice(index, 2, nextItem, item);
    dispatch({
      type: 'setField',
      field: {
        path: 'fields',
        value: tempItems
      }
    });
  };

  const getShifter = (direction: 'up' | 'down', index: number) => {
    switch (direction) {
      case 'up':
        return () => {
          moveUp(index);
        };
      case 'down':
        return () => {
          moveDown(index);
        };
    }
  };

  const addField = () => {
    dispatch({
      type: 'setField',
      field: {
        path: `fields[${items.length}]`,
        value: ''
      }
    });
  };

  return <ArangoTable style={{ marginLeft: 0 }}>
    {
      disabled
        ? null
        : <thead>
        <tr>
          <ArangoTH seq={0} style={{ width: '75%' }}/>
          <ArangoTH seq={1} style={{ width: '25%' }}>
            <button className={'button-warning'} onClick={addField}>
              <i className={'fa fa-plus'}/>&nbsp;Add Field
            </button>
          </ArangoTH>
        </tr>
        </thead>
    }
    <tbody>
    {
      items.length
        ? items.map((item, idx) => {
          const isLast = idx === items.length - 1;

          const updateField = (event: ChangeEvent<HTMLInputElement>) => {
            dispatch(
              {
                type: 'setField',
                field: {
                  path: `fields[${idx}]`,
                  value: event.target.value
                }
              }
            );
          };

          return <tr key={idx}>
            <ArangoTD seq={0}>
              <Textbox type={'text'} value={item} onChange={updateField} disabled={disabled}/>
            </ArangoTD>
            {
              disabled
                ? null
                : <ArangoTD seq={1}>
                  <StyledButtonSmall className={'button-danger'} onClick={getRemover(idx)}>
                    <StyledIcon className={'fa fa-trash-o'}/>
                  </StyledButtonSmall>&nbsp;
                  <StyledButtonSmall className={'button-warning'} onClick={getShifter('up', idx)}
                                     disabled={idx === 0}>
                    <StyledIcon className={'fa fa-arrow-up'}/>
                  </StyledButtonSmall>&nbsp;
                  <StyledButtonSmall className={'button-warning'} onClick={getShifter('down', idx)}
                                     disabled={isLast}>
                    <StyledIcon className={'fa fa-arrow-down'}/>
                  </StyledButtonSmall>
                </ArangoTD>
            }
          </tr>;
        })
        : <tr>
          <ArangoTD seq={0} colSpan={disabled ? 1 : 2}>No fields found.</ArangoTD>
        </tr>
    }
    </tbody>
  </ArangoTable>;
};

const StoredValuesInput = ({ formState, dispatch, disabled }: FormProps<StoredValues>) => {
  const items = formState.storedValues || [];

  const removeItem = (index: number) => {
    const tempItems = items.slice();

    tempItems.splice(index, 1);
    dispatch({
      type: 'setField',
      field: {
        path: 'storedValues',
        value: tempItems
      }
    });
  };

  const getRemover = (index: number) => () => {
    removeItem(index);
  };

  const addStoredValue = () => {
    const storedValue = {
      fields: [],
      compression: 'lz4'
    };

    dispatch({
      type: 'setField',
      field: {
        path: `storedValues[${items.length}]`,
        value: storedValue
      }
    });
  };

  const getWrappedDispatch = (index: number): Dispatch<DispatchArgs<StoredValues>> => (action: DispatchArgs<StoredValues>) => {
    action.basePath = getPath(`storedValues[${index}]`, action.basePath);
    (dispatch as unknown as Dispatch<DispatchArgs<StoredValues>>)(action);
  };

  const getColumnWidth = (index: number) => {
    const key = `${!!disabled}`;

    return `${columnWidths[key][index]}%`;
  };

  return <Fieldset legend={'Stored Values'}>
    <ArangoTable>
      <thead>
      <tr>
        <ArangoTH seq={0} style={{ width: getColumnWidth(0) }}>#</ArangoTH>
        <ArangoTH seq={1} style={{ width: getColumnWidth(1) }}>Fields</ArangoTH>
        <ArangoTH seq={2} style={{ width: getColumnWidth(2) }}>Compression</ArangoTH>
        {
          disabled
            ? null
            : <ArangoTH seq={3} style={{ width: getColumnWidth(3) }}>
              <button className={'button-warning'} onClick={addStoredValue}>
                <i className={'fa fa-plus'}/>&nbsp;Add
              </button>
            </ArangoTH>
        }
      </tr>
      </thead>
      <tbody>
      {
        items.length
          ? items.map((item, idx) => {
            const itemDispatch = getWrappedDispatch(idx);

            const updateCompression = (event: ChangeEvent<HTMLSelectElement>) => {
              itemDispatch(
                {
                  type: 'setField',
                  field: {
                    path: 'compression',
                    value: event.target.value
                  }
                }
              );
            };

            return <tr key={idx} style={{ borderBottom: '1px  solid #DDD' }}>
              <ArangoTD seq={0} valign={'middle'}>{idx + 1}.</ArangoTD>
              <ArangoTD seq={1}>
                <FieldsInput formState={item} dispatch={itemDispatch as Dispatch<DispatchArgs<StoredValue>>}
                             disabled={disabled}/>
              </ArangoTD>
              <ArangoTD seq={2}>
                <Select value={item.compression || 'lz4'} onChange={updateCompression} disabled={disabled}>
                  <option key={'lz4'} value={'lz4'}>LZ4</option>
                  <option key={'none'} value={'none'}>None</option>
                </Select>
              </ArangoTD>
              {
                disabled
                  ? null
                  : <ArangoTD seq={3}>
                    <button className={'button-danger'} onClick={getRemover(idx)}>
                      <StyledIcon className={'fa fa-trash-o'}/>
                    </button>
                  </ArangoTD>
              }
            </tr>;
          })
          : <tr>
            <ArangoTD seq={0} colSpan={disabled ? 3 : 4}>No stored values found.</ArangoTD>
          </tr>
      }
      </tbody>
    </ArangoTable>
  </Fieldset>;
};

export default StoredValuesInput;
