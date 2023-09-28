/* jshint unused: false */
/* global window, Joi */

(function () {
  "use strict";

  window.arangoValidationHelper = {
    getDocumentKeyRegex: () => {
      return /^[a-zA-Z0-9_\-:\.@()\+,=;$!*%']+$/;
    },
    getDocumentKeySpecialCharactersValidation: () => {
      var documentKeyRegex =
        window.arangoValidationHelper.getDocumentKeyRegex();
      var keySpecialCharactersValidation = {
        rule: Joi.string().regex(documentKeyRegex),
        msg: "Only these characters are allowed: a-z, A-Z, 0-9 and  _ - : . @ ( ) + , = ; $ ! * ' %.",
      };
      return keySpecialCharactersValidation;
    },
    getGraphNameValidations: () => {
      var keySpecialCharactersValidation =
        window.arangoValidationHelper.getDocumentKeySpecialCharactersValidation();

      var graphNameValidations = [
        keySpecialCharactersValidation,
        {
          rule: Joi.string().max(254, "utf8"),
          msg: "Graph name max length is 254 bytes.",
        },
        {
          rule: Joi.string().required(),
          msg: "No graph name given.",
        },
      ];
      return graphNameValidations;
    },
  };
})();
