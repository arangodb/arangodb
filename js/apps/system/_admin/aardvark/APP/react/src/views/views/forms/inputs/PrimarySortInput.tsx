import { DispatchArgs, FormProps } from "../../../../utils/constants";
import React, { ChangeEvent, Dispatch } from "react";
import Fieldset from "../../../../components/pure-css/form/Fieldset";
import { ArangoTable, ArangoTD, ArangoTH } from "../../../../components/arango/table";
import Textbox from "../../../../components/pure-css/form/Textbox";
import { getPath } from "../../../../utils/helpers";
import Select from "../../../../components/pure-css/form/Select";
import { PrimarySort } from "../../constants";
import { IconButton } from "../../../../components/arango/buttons";

const columnWidths: { [key: string]: number[] } = {
  'false': [5, 60, 10, 25],
  'true': [5, 85, 10, 0]
};

const PrimarySortInput = ({ formState, dispatch, disabled }: FormProps<PrimarySort>) => {
  const items = formState.primarySort || [];

  const removeItem = (index: number) => {
    const tempItems = items.slice();

    tempItems.splice(index, 1);
    dispatch({
      type: 'setField',
      field: {
        path: 'primarySort',
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
        path: 'primarySort',
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
        path: 'primarySort',
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

  const addPrimarySort = () => {
    const primarySort = {
      field: '',
      asc: true
    };

    dispatch({
      type: 'setField',
      field: {
        path: `primarySort[${items.length}]`,
        value: primarySort
      }
    });
  };

  const getWrappedDispatch = (index: number): Dispatch<DispatchArgs<PrimarySort>> => (action: DispatchArgs<PrimarySort>) => {
    action.basePath = getPath(`primarySort[${index}]`, action.basePath);
    (dispatch as unknown as Dispatch<DispatchArgs<PrimarySort>>)(action);
  };

  const getColumnWidth = (index: number) => {
    const key = `${!!disabled}`;

    return `${columnWidths[key][index]}%`;
  };

  return <Fieldset legend={'Primary Sort'}>
    <ArangoTable>
      <thead>
      <tr>
        <ArangoTH seq={0} style={{ width: getColumnWidth(0) }}>#</ArangoTH>
        <ArangoTH seq={1} style={{ width: getColumnWidth(1) }}>Field</ArangoTH>
        <ArangoTH seq={2} style={{ width: getColumnWidth(2) }}>Direction</ArangoTH>
        {
          disabled
            ? null
            : <ArangoTH seq={3} style={{ width: getColumnWidth(3) }}>
              <IconButton icon={'plus'} type={'warning'} onClick={addPrimarySort}>Add Primary
                Sort</IconButton>
            </ArangoTH>
        }
      </tr>
      </thead>
      <tbody>
      {
        items.length
          ? items.map((item, idx) => {
            const isFirst = idx === 0;
            const isLast = idx === items.length - 1;
            const itemDispatch = getWrappedDispatch(idx);

            const updateField = (event: ChangeEvent<HTMLInputElement>) => {
              itemDispatch(
                {
                  type: 'setField',
                  field: {
                    path: 'field',
                    value: event.target.value
                  }
                }
              );
            };

            const updateDirection = (event: ChangeEvent<HTMLSelectElement>) => {
              itemDispatch(
                {
                  type: 'setField',
                  field: {
                    path: 'asc',
                    value: event.target.value === 'asc'
                  }
                }
              );
            };

            const direction = item.asc ? 'asc' : 'desc';

            return <tr key={idx} style={{ borderBottom: '1px  solid #DDD' }}>
              <ArangoTD seq={0}>{idx + 1}.</ArangoTD>
              <ArangoTD seq={1}>
                <Textbox type={'text'} value={item.field} onChange={updateField}
                         disabled={disabled}/>
              </ArangoTD>
              <ArangoTD seq={2}>
                <Select value={direction} onChange={updateDirection} disabled={disabled}>
                  <option key={'asc'} value={'asc'}>ASC</option>
                  <option key={'desc'} value={'desc'}>DESC</option>
                </Select>
              </ArangoTD>
              {
                disabled
                  ? null
                  : <ArangoTD seq={3}>
                    <IconButton icon={'trash-o'} type={'danger'} onClick={getRemover(idx)}/>&nbsp;
                    <IconButton icon={'arrow-up'} type={'warning'} onClick={getShifter('up', idx)}
                                disabled={isFirst}/>&nbsp;
                    <IconButton icon={'arrow-down'} type={'warning'} onClick={getShifter('down', idx)}
                                disabled={isLast}/>
                  </ArangoTD>
              }
            </tr>;
          })
          : <tr>
            <ArangoTD seq={0} colSpan={disabled ? 3 : 4}>No primary sort definitions found.</ArangoTD>
          </tr>
      }
      </tbody>
    </ArangoTable>
  </Fieldset>;
};

export default PrimarySortInput;
