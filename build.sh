#!/bin/sh -x

configureOpts="--disable-sanitizers"

while getopts ":hd" opt; do
  case ${opt} in
    h ) echo "Use -d to turn on sanitizers (for debugging only)"
      exit;;
    d ) configureOpts=""
      ;;
    \? ) echo "Usage: $0 [-h] [-d]"
      exit;;
  esac
done

configureOpts="${configureOpts} --prefix=/usr --sysconfdir=/etc"

autoreconf -fiv

BUILD_DIR=build/
rm -rf $BUILD_DIR
mkdir -vp $BUILD_DIR
cd $BUILD_DIR || { echo "cd $BUILD_DIR"; exit 127; }

../configure ${configureOpts}

make
