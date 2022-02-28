import { DispatchArgs, FormProps } from "../../../utils/constants";
import { FormState, LinkProperties } from "../constants";
import React, { Dispatch, useEffect, useState } from "react";
import {
  ArangoTable,
  ArangoTD,
  ArangoTH
} from "../../../components/arango/table";
import { chain, difference, isEmpty, isNull, map } from "lodash";
import LinkPropertiesInput from "./inputs/LinkPropertiesInput";
import { IconButton } from "../../../components/arango/buttons";
import { useLinkState } from "../helpers";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import NewLink from "../Components/NewLink";
import LinkView from "../Components/LinkView";

const LinkPropertiesForm = ({
  formState,
  dispatch,
  disabled,
  view
}: FormProps<FormState>) => {
  const [collection, setCollection, addDisabled, links] = useLinkState(
    formState,
    "links"
  );
  const { data } = useSWR(["/collection", "excludeSystem=true"], (path, qs) =>
    getApiRouteForCurrentDB().get(path, qs)
  );
  const [options, setOptions] = useState<string[]>([]);

  const linkVal = chain(links)
    .omitBy(isNull)
    .keys()
    .value();

  useEffect(() => {
    if (data) {
      const linkKeys = chain(links)
        .omitBy(isNull)
        .keys()
        .value();
      const collNames = map(data.body.result, "name");
      const tempOptions = difference(collNames, linkKeys).sort();

      setOptions(tempOptions);
    }
  }, [data, links]);

  const updateCollection = (value: string | number) => {
    setCollection(value);
  };

  const addLink = () => {
    dispatch({
      type: "setField",
      field: {
        path: `links[${collection}]`,
        value: {}
      }
    });
    setCollection("");
  };

  const removeLink = (collection: string) => {
    dispatch({
      type: "setField",
      field: {
        path: `links[${collection}]`,
        value: null
      }
    });
  };

  const getLinkRemover = (collection: string) => () => {
    removeLink(collection);
  };

  return disabled && isEmpty(links) ? (
    <span>No links found.</span>
  ) : (
    <ArangoTable>
      <thead>
        <tr>
          <ArangoTH seq={disabled ? 0 : 1} style={{ width: "82%" }}>
            <a href={`/${view}`}>{view}</a>/
            <a href={`/${linkVal}`}>{linkVal}</a>
          </ArangoTH>
          <ArangoTH seq={disabled ? 1 : 2} style={{ width: "8%" }}>
            Collection Name
          </ArangoTH>
          {disabled ? null : (
            <ArangoTH
              seq={0}
              style={{
                width: "8%",
                textAlign: "center"
              }}
            >
              Action
            </ArangoTH>
          )}
        </tr>
      </thead>
      <tbody>
        {!disabled && linkVal.length < 1 && (
          <NewLink
            disabled={addDisabled || !options.includes(collection)}
            addLink={addLink}
            collection={collection}
            updateCollection={updateCollection}
            options={options}
          />
        )}

        <LinkView
          links={links}
          dispatch={
            (dispatch as unknown) as Dispatch<DispatchArgs<LinkProperties>>
          }
          disabled={disabled}
        />
        {map(links, (properties, coll) => {
          return properties ? (
            <tr key={coll} style={{ borderBottom: "1px  solid #DDD" }}>
              <ArangoTD seq={disabled ? 1 : 2}>
                <LinkPropertiesInput
                  formState={properties}
                  disabled={disabled || !properties}
                  dispatch={
                    (dispatch as unknown) as Dispatch<
                      DispatchArgs<LinkProperties>
                    >
                  }
                  basePath={`links[${coll}]`}
                />
              </ArangoTD>

              <ArangoTD seq={disabled ? 0 : 1}>{coll}</ArangoTD>
              {disabled ? null : (
                <ArangoTD seq={0} valign={"middle"}>
                  <IconButton
                    icon={"trash-o"}
                    type={"danger"}
                    onClick={getLinkRemover(coll)}
                  />
                </ArangoTD>
              )}
            </tr>
          ) : null;
        })}
      </tbody>
    </ArangoTable>
  );
};

export default LinkPropertiesForm;
