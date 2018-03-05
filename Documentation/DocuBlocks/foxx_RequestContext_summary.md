


`Route.summary(description)`

Set the summary for this route in the documentation.
Can't be longer than 8192 characters.
This is equal to using JavaDoc style comments right above your function.
If you provide both comment and `summary()` the call to `summary()` wins
and will be used.

*Examples*

Version with comment:

```js
/** Short description
 * 
 * Longer description
 * with multiple lines
 */
app.get("/foxx", function() {
});
```

is identical to:

```js
app.get("/foxx", function() {
})
.summary("Short description")
.notes(["Longer description", "with multiple lines"]); 
```


