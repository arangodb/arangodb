import "base-stemmer.jsx";

class Among
{
    var s : string;             /* search string */
    var substring_i : int;      /* index to longest matching substring */
    var result : int;           /* result of the lookup */
    var method : Nullable.<(BaseStemmer) -> boolean>;
                                /* method to use if substring matches */

    function constructor (s : string, substring_i : int, result : int)
    {
        this.s = s;
        this.substring_i = substring_i;
	this.result = result;
        this.method = null;
    }

    function constructor (s : string, substring_i : int, result : int,
                          method : (BaseStemmer) -> boolean)
    {
        this.s = s;
        this.substring_i = substring_i;
	this.result = result;
        this.method = method;
    }
}
