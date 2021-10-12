import { DispatchArgs, FormProps } from "../../../../utils/constants";
import { getPath } from "../../../../utils/helpers";
import { StoredValue, StoredValues } from "../../constants";
import Fieldset from "../../../../components/pure-css/form/Fieldset";
import { ArangoTable, ArangoTD, ArangoTH } from "../../../../components/arango/table";
import React, { ChangeEvent, Dispatch } from "react";
import Select from "../../../../components/pure-css/form/Select";
import StoredValueFieldsInput from "./StoredValueFieldsInput";
import { IconButton } from "../../../../components/arango/buttons";

const columnWidths: { [key: string]: number[] } = {
  'false': [5, 75, 10, 10],
  'true': [5, 85, 10, 0]
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
        <ArangoTH seq={0} style={{ width: getColumnWidth(0) }}><i className={'fa fa-trash-o'}/></ArangoTH>
        <ArangoTH seq={1} style={{ width: getColumnWidth(1) }}>Fields</ArangoTH>
        <ArangoTH seq={2} style={{ width: getColumnWidth(2) }}>Compression</ArangoTH>
        {
          disabled
            ? null
            : <ArangoTH seq={3} style={{ width: getColumnWidth(3) }}>
              <IconButton icon={'plus'} type={'warning'} onClick={addStoredValue}>Add</IconButton>
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
                <StoredValueFieldsInput formState={item} disabled={disabled}
                                        dispatch={itemDispatch as Dispatch<DispatchArgs<StoredValue>>}/>
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
                    <IconButton icon={'trash-o'} type={'danger'} onClick={getRemover(idx)}/>
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
