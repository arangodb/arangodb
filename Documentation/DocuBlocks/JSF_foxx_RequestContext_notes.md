


`Route.notes(...description)`

Set the long description for this route in the documentation

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


