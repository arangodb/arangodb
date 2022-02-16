import React, { useContext } from "react";
import { UrlParametersContext } from "./url-parameters-context";

const ParameterPokemon = () => {
  const urlParameters = useContext(UrlParametersContext);
  //console.log("urlParameters: ", urlParameters);

  /*
  const price = new Intl.NumberFormat("en-US", {
    style: "currency",
    currency: currency.code
  }).format(item.price * currency.conversionRate);
  */

  return (
    <>
      <button
        onClick={() => {
          console.log("urlParameters (Pokemon): ", urlParameters);
          console.log("Pokemon: ", urlParameters.pokemon);
          urlParameters.pokemon = "ditto";
        }}
      >
        Change Pokemon to Ditto
      </button>
      <br />
      <strong>Pokemon: {urlParameters.pokemon}</strong>
      <hr />
    </>
  );
};

export default ParameterPokemon;
