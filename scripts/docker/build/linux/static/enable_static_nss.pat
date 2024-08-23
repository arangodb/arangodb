--- debian/rules.d/build.mk.orig	2023-12-05 20:49:26.513919555 +0100
+++ debian/rules.d/build.mk	2023-12-05 20:49:47.054342582 +0100
@@ -109,6 +109,7 @@
 		--without-selinux \
 		--enable-stackguard-randomization \
 		--enable-stack-protector=strong \
+		--enable-static-nss=yes \
 		--with-pkgversion="Ubuntu GLIBC $(DEB_VERSION)" \
 		--with-default-link=no \
 		--with-bugurl="https://bugs.launchpad.net/ubuntu/+source/glibc/+bugs" \
