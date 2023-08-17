/* jshint unused: false */
/* global window, Joi */

(function () {
  "use strict";

  window.arangoValidationHelper = {
    getControlCharactersRegex: () => {
      return /^[^\u0000-\u001F]+$/;
    },
    getDocumentKeyRegex: () => {
      return /^[a-zA-Z0-9_\-:\.@()\+,=;$!*%']+$/;
    },
    getDatabaseNameValidations: () => {
      var extendedNames = window.frontendConfig.extendedNames;
      var traditionalNameValidation = [
        {
          rule: Joi.string().regex(/^[a-zA-Z]/),
          msg: "Database name must always start with a letter.",
        },
        {
          rule: Joi.string().regex(/^[a-zA-Z0-9\-_]*$/),
          msg: 'Only symbols, "_" and "-" are allowed.',
        },
        {
          rule: Joi.string().max(64, "utf8"),
          msg: "Database name max length is 64.",
        },
      ];

      var extendedValidation = [
        {
          rule: Joi.string().regex(/^(?![0-9._])/),
          msg: "Database name cannot start with a number, a dot (.), or an underscore (_).",
        },
        {
          rule: Joi.string().regex(
            window.arangoValidationHelper.getControlCharactersRegex()
          ),
          msg: "Database name cannot contain control characters (0-31).",
        },
        {
          rule: Joi.string().regex(/^(?!.*[/:])/),
          msg: "Database name cannot contain a forward slash (/) or a colon (:)",
        },
        {
          rule: Joi.string().regex(/^\S(.*\S)?$/),
          msg: "Database name cannot contain leading/trailing spaces.",
        },
        {
          rule: Joi.string().normalize().max(128, "utf8"),
          msg: "Database name max length is 128 bytes.",
        },
      ];
      var databaseNameValidations = extendedNames
        ? extendedValidation
        : traditionalNameValidation;
      databaseNameValidations = databaseNameValidations.concat({
        rule: Joi.string().required(),
        msg: "No database name given.",
      });
      return databaseNameValidations;
    },
    getDocumentNameValidations: () => {
      var documentKeyRegex =
        window.arangoValidationHelper.getDocumentKeyRegex();
      var keySpecialCharactersValidation = {
        rule: Joi.string().regex(documentKeyRegex).allow(""),
        msg: "Only these characters are allowed: a-z, A-Z, 0-9 and  _ - : . @ ( ) + , = ; $ ! * ' %.",
      };
      return [
        keySpecialCharactersValidation,
        {
          rule: Joi.string().max(254, "utf8").allow(""),
          msg: "Document key max length is 254.",
        },
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
          rule: Joi.string().normalize().max(256, "utf8"),
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
