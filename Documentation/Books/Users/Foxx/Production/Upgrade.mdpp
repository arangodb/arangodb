!CHAPTER Upgrade to a new version

In this chapter we want to cover the case where you have a Foxx in production and want to upgrade it to a new version.
In this specific case you do not want to execute the setup and teardown scripts.
A simple uninstall / install procedure would first trigger both scripts and would trigger a very short downtime of your app.
Therefore the Foxx-manager offers an update procedure, which acts similar to install, but overwrites the existing Foxx.
In addition it does neither trigger setup nor teardown.

```
unix> foxx-manager update itzpapalotl:1.0.0 /example
Application itzpapalotl version 1.0.0 installed successfully at mount point /example
```

Also you will not experience any downtime of your Foxx.
One thread will be responsible to update to the newer version, all other threads will continue to serve the old version.
As soon as the update thread is finished rerouting of all other threads is scheduled as well and eventually they will converge to the newest version of the app:
All requests issued before the rerouting scheduling will be answered with the old version, all requests issued after will be answered with the new version.
