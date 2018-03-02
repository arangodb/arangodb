Install Applications from Github
================================

In this chapter we will make use of the Foxx manager as described [before](README.md).
This time we want to install an app out of our version control hosted on [github.com](https://www.github.com).

In order to install an application we need three informations:

* **Repository**: The name of the repository.
* **Username**: The username of the user owning the repository.
* **Version**: (optional) branch or tag available on the repository.

As an example, we would like to install [www.github.com/arangodb/hello-foxx](https://www.github.com/arangodb/hello-foxx).
The **username** is **arangodb**, the **repository** is **hello-foxx**.
If we do not define a **version** it will automatically install the master branch.

```
unix> foxx-manager install git:arangodb/hello-foxx /example
Application hello-foxx version 1.5.0 installed successfully at mount point /example
```

The hello-foxx app has defined a tag for version 1.4.4 that is named "v1.4.4".
We can simply append this tag in the install command:

```
unix> foxx-manager install git:arangodb/hello-foxx:v1.4.4 /legacy
Application hello-foxx version 1.4.4 installed successfully at mount point /legacy
```

This reference for github repositories can be used in all functions of the Foxx-manager that allow to install Foxx applications:

**install**

```
unix> foxx-manager install git:arangodb/hello-foxx:v1.4.4 /legacy
Application hello-foxx version 1.4.4 installed successfully at mount point /legacy
```

**replace**

```
unix> foxx-manager replace git:arangodb/hello-foxx:v1.4.4 /legacy
Application hello-foxx version 1.4.4 installed successfully at mount point /legacy
```

**upgrade**

```
unix> foxx-manager upgrade git:arangodb/hello-foxx:v1.5.0 /legacy
Application hello-foxx version 1.5.0 installed successfully at mount point /legacy
```
