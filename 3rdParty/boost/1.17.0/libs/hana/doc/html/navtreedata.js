/*
@ @licstart  The following is the entire license notice for the
JavaScript code in this file.

Copyright (C) 1997-2017 by Dimitri van Heesch

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

@licend  The above is the entire license notice
for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "Boost.Hana", "index.html", [
    [ "User Manual", "index.html", [
      [ "Description", "index.html#tutorial-description", null ],
      [ "Prerequisites and installation", "index.html#tutorial-installation", [
        [ "Note for CMake users", "index.html#tutorial-installation-cmake", null ],
        [ "Compiler requirements", "index.html#tutorial-installation-requirements", null ]
      ] ],
      [ "Support", "index.html#tutorial-support", null ],
      [ "Introduction", "index.html#tutorial-introduction", [
        [ "C++ computational quadrants", "index.html#tutorial-introduction-quadrants", null ],
        [ "What is this library about?", "index.html#tutorial-quadrants-about", null ]
      ] ],
      [ "Quick start", "index.html#tutorial-quickstart", [
        [ "A real world example", "index.html#tutorial-quickstart-any", null ]
      ] ],
      [ "Cheatsheet", "index.html#tutorial-cheatsheet", null ],
      [ "Assertions", "index.html#tutorial-assert", null ],
      [ "Compile-time numbers", "index.html#tutorial-integral", [
        [ "Compile-time arithmetic", "index.html#tutorial-integral-arithmetic", null ],
        [ "Example: Euclidean distance", "index.html#tutorial-integral-distance", null ],
        [ "Compile-time branching", "index.html#tutorial-integral-branching", null ],
        [ "Why stop here?", "index.html#tutorial-integral-more", null ]
      ] ],
      [ "Type computations", "index.html#tutorial-type", [
        [ "Types as objects", "index.html#tutorial-type-objects", null ],
        [ "Benefits of this representation", "index.html#tutorial-type-benefits", null ],
        [ "Working with this representation", "index.html#tutorial-type-working", null ],
        [ "The generic lifting process", "index.html#tutorial-type-lifting", null ]
      ] ],
      [ "Introspection", "index.html#tutorial-introspection", [
        [ "Checking expression validity", "index.html#tutorial-introspection-is_valid", [
          [ "Non-static members", "index.html#tutorial-introspection-is_valid-non_static", null ],
          [ "Static members", "index.html#tutorial-introspection-is_valid-static", null ],
          [ "Nested type names", "index.html#tutorial-introspection-is_valid-nested-typename", null ],
          [ "Nested templates", "index.html#tutorial-introspection-is_valid-nested-template", null ],
          [ "Template specializations", "index.html#tutorial-introspection-is_valid-template", null ]
        ] ],
        [ "Taking control of SFINAE", "index.html#tutorial-introspection-sfinae", null ],
        [ "Introspecting user-defined types", "index.html#tutorial-introspection-adapting", null ],
        [ "Example: generating JSON", "index.html#tutorial-introspection-json", null ]
      ] ],
      [ "Generalities on containers", "index.html#tutorial-containers", [
        [ "Container creation", "index.html#tutorial-containers-creating", null ],
        [ "Container types", "index.html#tutorial-containers-types", [
          [ "Overloading on container types", "index.html#tutorial-containers-types-overloading", null ]
        ] ],
        [ "Container elements", "index.html#tutorial-containers-elements", null ]
      ] ],
      [ "Generalities on algorithms", "index.html#tutorial-algorithms", [
        [ "By-value semantics", "index.html#tutorial-algorithms-value", null ],
        [ "(Non-)Laziness", "index.html#tutorial-algorithms-laziness", null ],
        [ "What is generated?", "index.html#tutorial-algorithms-codegen", null ],
        [ "Side effects and purity", "index.html#tutorial-algorithms-effects", null ],
        [ "Cross-phase algorithms", "index.html#tutorial-algorithms-cross_phase", null ]
      ] ],
      [ "Performance considerations", "index.html#tutorial-performance", [
        [ "Compile-time performance", "index.html#tutorial-performance-compile", null ],
        [ "Runtime performance", "index.html#tutorial-performance-runtime", null ]
      ] ],
      [ "Integration with external libraries", "index.html#tutorial-ext", null ],
      [ "Hana's core", "index.html#tutorial-core", [
        [ "Tags", "index.html#tutorial-core-tags", null ],
        [ "Tag dispatching", "index.html#tutorial-core-tag_dispatching", null ],
        [ "Emulation of C++ concepts", "index.html#tutorial-core-concepts", null ]
      ] ],
      [ "Header organization", "index.html#tutorial-header_organization", null ],
      [ "Conclusion", "index.html#tutorial-conclusion", [
        [ "Fair warning: functional programming ahead", "index.html#tutorial-conclusion-warning", null ],
        [ "Related material", "index.html#tutorial-conclusion-related_material", null ],
        [ "Projects using Hana", "index.html#tutorial-conclusion-projects_using_hana", null ]
      ] ],
      [ "Using the reference", "index.html#tutorial-reference", [
        [ "Function signatures", "index.html#tutorial-reference-signatures", null ]
      ] ],
      [ "Acknowledgements", "index.html#tutorial-acknowledgements", null ],
      [ "Glossary", "index.html#tutorial-glossary", null ],
      [ "Rationales/FAQ", "index.html#tutorial-rationales", [
        [ "Why restrict usage of external dependencies?", "index.html#tutorial-rationales-dependencies", null ],
        [ "Why no iterators?", "index.html#tutorial-rationales-iterators", null ],
        [ "Why leave some container's representation implementation-defined?", "index.html#tutorial-rationales-container_representation", null ],
        [ "Why Hana?", "index.html#tutorial-rationales-why_Hana", null ],
        [ "Why define our own tuple?", "index.html#tutorial-rationales-tuple", null ],
        [ "How are names chosen?", "index.html#tutorial-rationales-naming", null ],
        [ "How is the parameter order decided?", "index.html#tutorial-rationales-parameters", null ],
        [ "Why tag dispatching?", "index.html#tutorial-rationales-tag_dispatching", null ],
        [ "Why not provide zip_longest?", "index.html#tutorial-rationales-zip_longest", null ],
        [ "Why aren't concepts constexpr functions?", "index.html#tutorial-rationales-concepts", null ]
      ] ],
      [ "Appendix I: Advanced constexpr", "index.html#tutorial-appendix-constexpr", [
        [ "Constexpr stripping", "index.html#tutorial-appendix-constexpr-stripping", null ],
        [ "Constexpr preservation", "index.html#tutorial-tutorial-appendix-constexpr-preservation", null ],
        [ "Side effects", "index.html#tutorial-appendix-constexpr-effects", null ]
      ] ]
    ] ],
    [ "Reference documentation", "modules.html", "modules" ],
    [ "Alphabetical index", "functions.html", null ],
    [ "Headers", "files.html", "files" ],
    [ "Todo List", "todo.html", null ],
    [ "Deprecated List", "deprecated.html", null ],
    [ "Bug List", "bug.html", null ]
  ] ]
];

var NAVTREEINDEX =
[
"accessors_8hpp.html",
"fwd_2count_8hpp.html",
"group__group-Comonad.html#ga181751278bd19a4bfc3c08bd7ddef399",
"group__group-functional.html#ga41ada6b336e9d5bcb101ff0c737acbd0",
"structboost_1_1hana_1_1integral__constant.html#a79f45e3c2411db1d36127c1341673ffb"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';