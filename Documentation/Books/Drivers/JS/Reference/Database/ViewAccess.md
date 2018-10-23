<!-- don't edit here, it's from https://@github.com/arangodb/arangojs.git / docs/Drivers/ -->
# Accessing views

These functions implement the
[HTTP API for accessing views](../../../..//HTTP/Views/Getting.html).

## database.arangoSearchView

`database.arangoSearchView(viewName): ArangoSearchView`

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.4 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Returns a _ArangoSearchView_ instance for the given view name.

**Arguments**

- **viewName**: `string`

  Name of the arangosearch view.

**Examples**

```js
const db = new Database();
const view = db.arangoSearchView("potatoes");
```

## database.listViews

`async database.listViews(): Array<Object>`

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.4 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Fetches all views from the database and returns an array of view
descriptions.

**Examples**

```js
const db = new Database();

const views = await db.listViews();
// views is an array of view descriptions
```

## database.views

`async database.views([excludeSystem]): Array<View>`

{% hint 'info' %}
This method is only available when targeting ArangoDB 3.4 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Fetches all views from the database and returns an array of
_ArangoSearchView_ instances for the views.

**Examples**

```js
const db = new Database();

const views = await db.views();
// views is an array of ArangoSearchView instances
```
