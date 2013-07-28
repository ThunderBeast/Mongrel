#! /bin/sh

cd ..
find ./ -name "*.[ch]" > ./uncrustify/files.txt
cd uncrustify

files=$(cat files.txt)

for item in $files ; do

  dn=$(dirname $item)
  mkdir -p out/$dn
  ./uncrustify -f ../$item -c default.cfg > out/$item

done

