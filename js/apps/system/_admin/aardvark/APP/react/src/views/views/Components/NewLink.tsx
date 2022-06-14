import React, { useContext, useEffect, useState } from "react";
import ViewLinkLayout from "./ViewLinkLayout";
import { ArangoTable, ArangoTD } from "../../../components/arango/table";
import AutoCompleteTextInput from "../../../components/pure-css/form/AutoCompleteTextInput";
import { IconButton } from "../../../components/arango/buttons";
import { ViewContext } from "../constants";
import { useHistory, useLocation } from "react-router-dom";
import { NavButton } from "../Actions";
import { useLinkState } from "../helpers";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import { chain, difference, isNull, map } from "lodash";

const NewLink = () => {
  const { dispatch, formState } = useContext(ViewContext);
  const history = useHistory();
  const location = useLocation();
  const [collection, setCollection, addDisabled, links] = useLinkState(formState, 'links');
  const { data } = useSWR(['/collection', 'excludeSystem=true'], (path, qs) =>
    getApiRouteForCurrentDB().get(path, qs)
  );
  const [options, setOptions] = useState<string[]>([]);

  useEffect(() => {
    if (data) {
      const linkKeys = chain(links)
        .omitBy(isNull)
        .keys()
        .value();
      const collNames = map(data.body.result, 'name');
      const tempOptions = difference(collNames, linkKeys).sort();

      setOptions(tempOptions);
    }
  }, [data, links]);

  const up = location.pathname.slice(0, location.pathname.lastIndexOf('/'));

  const addLink = () => {
    dispatch({
      type: "setField",
      field: {
        path: `links[${collection}]`,
        value: {}
      }
    });
    history.push(up);
  };

  return (
    <ViewLinkLayout fragments={['[New Link]']}>
      <ArangoTable>
        <tbody>
        <tr style={{ borderBottom: "1px  solid #DDD" }}>
          <ArangoTD seq={0} colSpan={2}>
            <AutoCompleteTextInput placeholder={"Collection"} value={collection} minChars={1}
                                   spacer={""} onSelect={setCollection} matchAny={true} options={options}
                                   onChange={setCollection}/>
            { collection && addDisabled ? <div style={{ color: 'red' }}>This link already exists</div> : null }
            { !collection || options.includes(collection) ? null : addDisabled ? null : <div style={{ color: 'red' }}>This collection does not exist</div> }
          </ArangoTD>
          <ArangoTD seq={1} style={{ width: "20%" }}>
            <IconButton icon={"plus"} type={"warning"} onClick={addLink}
                        disabled={addDisabled || !options.includes(collection)}>
              Add
            </IconButton>
          </ArangoTD>
        </tr>
        </tbody>
        <tfoot>
        <tr><ArangoTD seq={0}><NavButton arrow={'left'} text={'Back'}/></ArangoTD></tr>
        </tfoot>
      </ArangoTable>
    </ViewLinkLayout>
  );
};

export default NewLink;
