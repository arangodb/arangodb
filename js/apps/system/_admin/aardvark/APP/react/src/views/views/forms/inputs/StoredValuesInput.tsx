import { DispatchArgs, FormProps } from "../../../../utils/constants";
import { getPath } from "../../../../utils/helpers";
import { StoredValues } from "../../constants";
import Fieldset from "../../../../components/pure-css/form/Fieldset";
import { ArangoTable, ArangoTD, ArangoTH } from "../../../../components/arango/table";
import React, { ChangeEvent, Dispatch } from "react";
import Textarea from "../../../../components/pure-css/form/Textarea";
import { filter, isEmpty, negate } from "lodash";
import Select from "../../../../components/pure-css/form/Select";
import styled from "styled-components";

const StyledButton = styled.button`
  &&& {
    margin-top: 10px;
  }
`;
const StyledIcon = styled.i`
  &&& {
    margin-left: auto;
  }
`;

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

  const moveUp = (index: number) => {
    const tempItems = items.slice();
    const item = tempItems[index];
    const prevItem = tempItems[index - 1];

    tempItems.splice(index - 1, 2, item, prevItem);
    dispatch({
      type: 'setField',
      field: {
        path: 'storedValues',
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
        path: 'storedValues',
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

  return <Fieldset legend={'Stored Values'}>
    <ArangoTable>
      <thead>
      <tr>
        <ArangoTH seq={0}>#</ArangoTH>
        <ArangoTH seq={1}>Fields</ArangoTH>
        <ArangoTH seq={2}>Compression</ArangoTH>
        {
          disabled
            ? null
            : <ArangoTH seq={3}>
              <button className={'button-warning'} onClick={addStoredValue}>
                <i className={'fa fa-plus'}/>&nbsp;Add
              </button>
            </ArangoTH>
        }
      </tr>
      </thead>
      <tbody>
      {
        items.map((item, idx) => {
          const isLast = idx === items.length - 1;
          const itemDispatch = getWrappedDispatch(idx);

          const updateFields = (event: ChangeEvent<HTMLTextAreaElement>) => {
            const fields = event.target.value.split('\n');

            if (filter(fields, negate(isEmpty)).length) {
              itemDispatch({
                type: 'setField',
                field: {
                  path: 'fields',
                  value: fields
                }
              });
            } else {
              itemDispatch({
                type: 'unsetField',
                field: {
                  path: 'fields'
                }
              });
            }
          };

          const getFields = () => {
            return (item.fields || []).join('\n');
          };

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
              <Textarea value={getFields()} onChange={updateFields} disabled={disabled}/>
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
                  <StyledButton className={'button-danger'} onClick={getRemover(idx)}>
                    <StyledIcon className={'fa fa-trash-o'}/>
                  </StyledButton>&nbsp;
                  <StyledButton className={'button-warning'} onClick={getShifter('up', idx)}
                                disabled={idx === 0}>
                    <StyledIcon className={'fa fa-arrow-up'}/>
                  </StyledButton>&nbsp;
                  <StyledButton className={'button-warning'} onClick={getShifter('down', idx)}
                                disabled={isLast}>
                    <StyledIcon className={'fa fa-arrow-down'}/>
                  </StyledButton>
                </ArangoTD>
            }
          </tr>;
        })
      }
      </tbody>
    </ArangoTable>
  </Fieldset>;
};

export default StoredValuesInput;
