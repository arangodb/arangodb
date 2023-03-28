import React, { useContext, useEffect, useState } from "react";
import { FormState, ViewContext, ViewProps } from "../constants";
import { SaveButton } from "../Actions";
import { chain, difference, isNull, map, without } from "lodash";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import AutoCompleteMultiSelect from "../../../components/pure-css/form/AutoCompleteMultiSelect";
import { Cell, Grid } from "../../../components/pure-css/grid";
import { Link, useRouteMatch } from "react-router-dom";

const LinkList = ({ name }: ViewProps) => {
  const { dispatch, formState: fs, isAdminUser, changed, setChanged } = useContext(ViewContext);
  const formState = fs as FormState;
  const { data } = useSWR(['/collection', 'excludeSystem=true'], (path, qs) =>
    getApiRouteForCurrentDB().get(path, qs)
  );
  const [options, setOptions] = useState<string[]>([]);
  const match = useRouteMatch();

  useEffect(() => {
    if (data) {
      const linkKeys = chain(formState.links)
        .omitBy(isNull)
        .keys()
        .value();
      const collNames = map(data.body.result, 'name');
      const tempOptions = difference(collNames, linkKeys).sort();

      setOptions(tempOptions);
    }
  }, [data, formState.links]);

  const addLink = (link: string | number) => {
    dispatch({
      type: "setField",
      field: {
        path: `links[${link}]`,
        value: {}
      }
    });
    setOptions(without(options, link as string));
  };

  const removeLink = async (link: string | number) => {
    dispatch({
      type: "setField",
      field: {
        path: `links[${link}]`,
        value: null
      }
    });
    setOptions(options.concat([link as string]).sort());
  };

  const validLinks = chain(formState.links).toPairs().filter(pair => pair[1] !== null).map(pair => ({
    key: pair[0],
    value: <Link to={`${match.url}${pair[0]}`}>{pair[0]}</Link>
  })).value();

  return <div id="modal-dialog" style={{
    width: 1024,
    marginLeft: 'auto',
    marginRight: 'auto'
  }}>
    <div className="modal-body" style={{
      minHeight: 300,
      border: 'none',
      height: 'fit-content'
    }}>
      <Grid>
        <Cell size={"1"}>
          <AutoCompleteMultiSelect
            values={validLinks}
            onRemove={removeLink}
            onSelect={addLink}
            options={options}
            label={'Links'}
            disabled={!isAdminUser}
            errorMsg={'Collection does not exist'}
            placeholder={'Enter a collection name'}
          />
        </Cell>
        {
          isAdminUser && changed
            ? <Cell size={"1"}>
              <SaveButton view={formState} oldName={name} setChanged={setChanged}/>
            </Cell>
            : null
        }
      </Grid>
    </div>
  </div>;
};

export default LinkList;
