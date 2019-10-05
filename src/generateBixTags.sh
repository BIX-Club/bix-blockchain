#!/bin/bash
for x in a b c d e f g h i j k l m n o p q r s t u v w x y z
do
for y in a b c d e f g h i j k l m n o p q r s t u v w x y z
do
for z in a b c d e f g h i j k l m n o p q r s t u v w x y z
do
		echo "#define BIX_TAG_$x$y$z BIX_TAG('$x', '$y', '$z')"
done
done
done
