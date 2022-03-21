
@startDocuBlock GetApiQueryRules
@brief returns a list of all available optimizer rules for AQL queries

@RESTHEADER{GET /_api/query/rules, Returns all AQL optimizer rules, queryRules}

@RESTDESCRIPTION
Returns an array of which each element describes an AQL optimizer rule.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the list of optimizer rules can be retrieved successfully.

@RESTREPLYBODY{,array,required,get_api_query_rules}
A list of all optimizer rules and their properties.

@RESTSTRUCT{name,get_api_query_rules,string,required,}
The name of the optimizer rule.

@RESTSTRUCT{flags,get_api_query_rules,object,required,get_api_query_rules_flags}
Properties of the rule.

@RESTSTRUCT{hidden,get_api_query_rules_flags,boolean,required,}
Whether the rule is displayed to users. Internal rules are hidden.

@RESTSTRUCT{clusterOnly,get_api_query_rules_flags,boolean,required,}
Whether the rule is applicable in cluster deployment mode only.

@RESTSTRUCT{canBeDisabled,get_api_query_rules_flags,boolean,required,}
Whether users are allowed to disable this rule. A few rules are mandatory.

@RESTSTRUCT{canCreateAdditionalPlans,get_api_query_rules_flags,boolean,required,}
Whether this rule may create additional query execution plans.

@RESTSTRUCT{disabledByDefault,get_api_query_rules_flags,boolean,required,}
Whether the optimizer considers this rule by default.

@RESTSTRUCT{enterpriseOnly,get_api_query_rules_flags,boolean,required,}
Whether the rule is available in the Enterprise Edition only.

@EXAMPLES

Execute a query and extract the result in a single go

@EXAMPLE_ARANGOSH_RUN{RestQueryRules}
    var url = "/_api/query/rules";
    var response = logCurlRequest('GET', url);
    assert(response.code === 200);
    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
