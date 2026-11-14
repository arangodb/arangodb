--- debian/rules.d/build.mk.orig	2026-11-13 12:48:14.077380837 +0000
+++ debian/rules.d/build.mk	2026-11-13 12:48:14.093380838 +0000
@@ -118,6 +118,7 @@
 		--enable-bind-now \
 		--enable-fortify-source \
 		--enable-stackguard-randomization \
+		--enable-static-nss=yes \
 		--with-pkgversion="Ubuntu GLIBC $(DEB_VERSION)" \
 		--with-default-link=no \
 		--with-bugurl="https://bugs.launchpad.net/ubuntu/+source/glibc/+bugs" \
