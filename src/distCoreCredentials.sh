#!/bin/bash
if test "$1" = ""
then
  echo
  echo "  usage: $0 <corenumber> <destination>"
  echo
  echo "  Example: $0 05 user@remotehost.com"
  echo
  exit 1
fi

cd ..
tar cvz data/*/$1 data/servertopology.json | ssh $2 "cd bix-blockchain; tar xz"

