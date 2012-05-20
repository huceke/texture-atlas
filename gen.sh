#!/bin/sh

WORK=$PWD
OUTPUT=$WORK/Textures.xml

./texture-atlas media

echo "<atlasmap>" > $OUTPUT

for i in media-2048*.txt
do
  filename=$(echo $i | sed -e 's/.txt/.png/g')
  while read line
  do
    set -- $line
    echo "  <texture>" >> $OUTPUT
    texture=$(echo $1 | sed -e s'/media\///g')
    echo "    <filename>$filename</filename>" >> $OUTPUT
    echo "    <texturename>$texture</texturename>" >> $OUTPUT
    echo "    <x>$2</x>" >> $OUTPUT
    echo "    <y>$3</y>" >> $OUTPUT
    echo "    <width>$4</width>" >> $OUTPUT
    echo "    <height>$5</height>" >> $OUTPUT
    echo "  </texture>" >> $OUTPUT
  done < $i
done

echo "</atlasmap>" >> $OUTPUT
