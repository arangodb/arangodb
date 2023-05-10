/* jshint unused: false */
(function () {
  "use strict";

  window.arangoValidationHelper = {
    getControlCharactersRegex: () => {
      return /[^\u0000-\u001F]/;
    },
    getDocumentKeySpecialCharactersValidation: () => {
      var keySpecialCharactersValidation = {
        rule: Joi.string().regex(/^[a-zA-Z0-9_\-:\.@()\+,=;$!*\%']+$/),
        msg: "Only these characters are allowed: a-z, A-Z, 0-9 and  _ - : . @ ( ) + , = ; $ ! * ' %.",
      };
      return keySpecialCharactersValidation;
    },
    getDocumentNameValidations: () => {
      var keySpecialCharactersValidation =
        window.arangoValidationHelper.getDocumentKeySpecialCharactersValidation();
      return [
        keySpecialCharactersValidation,
        {
          rule: Joi.string().allow("").optional(),
          msg: "",
        },
      ];
    },
    getCollectionNameValidations: () => {
      var traditionalCollectionNameValidation = [
        {
          rule: Joi.string().regex(/^[a-zA-Z]/),
          msg: "Collection name must always start with a letter.",
        },
        {
          rule: Joi.string().regex(/^[a-zA-Z0-9\-_]*$/),
          msg: 'Only symbols, "_" and "-" are allowed.',
        },
      ];
      var extendedCollectionNameValidation = [
        {
          rule: Joi.string().regex(/^(?![0-9._])/),
          msg: "Collection name cannot start with a number, a dot (.), or an underscore (_).",
        },
        {
          rule: Joi.string().regex(
            window.arangoValidationHelper.getControlCharactersRegex()
          ),
          msg: "Collection name cannot contain control characters (0-31).",
        },
        {
          rule: Joi.string().regex(/^\S(.*\S)?$/),
          msg: "Collection name cannot contain leading/trailing spaces.",
        },
        {
          rule: Joi.string().regex(/^(?!.*[/])/),
          msg: "Collection name cannot contain a forward slash (/).",
        },
      ];
      var extendedNames = window.frontendConfig.extendedNames;

      let collectionNameValidations = extendedNames
        ? extendedCollectionNameValidation
        : traditionalCollectionNameValidation;
      collectionNameValidations = collectionNameValidations.concat(
        {
          rule: Joi.normalize().string().max(256, "utf8"),
          msg: "Collection name max length is 256.",
        },
        {
          rule: Joi.string().required(),
          msg: "No collection name given.",
        }
      );
      return collectionNameValidations;
    },
  };
})();
