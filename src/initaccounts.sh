#!/bin/bash

maxaccounts=10
if test "$1" != ""
then
  maxaccounts=$1
fi


echo "Creating keys"
s=0
while test $s -lt $maxaccounts
do
  cks=`expr $s % 10`
  si=`printf "%02d" $s`
  sa=`printf "1%o80%01d" $s $cks`
  path="../data/accounts/$si/$sa"
  echo $path
  mkdir -p $path/notes
  ( 
  	cd $path
	openssl ecparam -name secp521r1 -genkey -noout -out private.pem
	openssl ec -in private.pem -pubout -out public.pem 
  )
  s=`expr $s + 1`
done

echo "Distributing keys"
s=0
while test $s -lt $maxaccounts
do
  cks=`expr $s % 10`
  si=`printf "%02d" $s`
  sa=`printf "1%o80%01d" $s $cks`
  path="../data/accounts/$si/$sa"

  d=0
  while test $d -lt $maxaccounts
  do
	di=`printf "%02d" $d`
    dpath="../data/accounts/$di/$sa"
	mkdir -p $dpath/notes
	(
	  cd $dpath
	  echo 1 > approved.txt
	  echo 0 > balance-BIX.txt
	  # echo -n BIX > token.txt
	  echo "`date`: Account initialized by init script. " >> notes/00000.txt      
	)
    if test "$path" != "$dpath"
    then
      cp $path/public.pem $dpath/
    fi
    d=`expr $d + 1`
  done

  s=`expr $s + 1`
done
