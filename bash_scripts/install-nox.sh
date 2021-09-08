#!/bin/bash

sudo apt-get update
sudo apt-get install -y build-essential autoconf libtool libssl-dev libtbb-dev libboost-all-dev

sudo ldconfig

git clone https://github.com/noxrepo/nox.git
cd nox
./boot.sh
ln -s config/install-sh install-sh
mkdir build
cd build
../configure --with-boost-libdir=/usr/lib/x86_64-linux-gnu
make -j
cd src


#
# MAY NEED THIS PATCHES
#
#diff --git a/configure.ac.in b/configure.ac.in
#index f4d8f42..0ef70ef 100644
#--- a/configure.ac.in
#+++ b/configure.ac.in
#@@ -8,7 +8,6 @@ AC_SYS_LARGEFILE
#
# AC_CONFIG_SRCDIR([src])
# AC_CONFIG_HEADERS([config.h])
#-AC_CONFIG_AUX_DIR([config])
# AC_CONFIG_MACRO_DIR([m4])
# LT_INIT([dlopen])
# LT_LANG([C++])
#@@ -17,7 +16,7 @@ NX_BUILDNR
# AH_BOTTOM([/* NOX 0.9.1~APPS_ID~beta. */
# #define NOX_VERSION VERSION BUILDNR_SUFFIX])
#
#-AM_INIT_AUTOMAKE([tar-ustar -Wno-portability])
#+AM_INIT_AUTOMAKE([tar-ustar -Wno-portability subdir-objects])
#
# # Checks for programs.
# AC_PROG_CXX
#@@ -93,7 +93,7 @@ DX_INIT_DOXYGEN(nox, doc/doxygen.conf, doc)
# # Add your module here to have it grouped into a package
#
# ACI_PACKAGE([coreapps],[core application set],
#-               [ openflow switch
#+               [ openflow switch discovery
#                  #add coreapps component here
#                ],
#                [yes])
#

