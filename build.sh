#!/bin/sh -e

cd `dirname $0`

echo "== Building libnem"
./libnem/build.sh

echo "== Building libnemsvc"
./libnemsvc/build.sh

echo "== Building nem-rootd"
./nem-rootd/build.sh
