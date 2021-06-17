#!/bin/bash

RST=result.txt
if [ -f $RST ];then
	rm $RST
fi
touch $RST

echo "If the CMD runs incorrectly, return value will put in $RST" > $RST
echo -e "Else nothing will put in $RST\n" >> $RST
echo "TEST START" >> $RST

cat ./busybox_cmd.txt | while read line
do
	./busybox $line
	RTN=$?
	if [[ $RTN -ne 0 && $line != "false" ]] ;then
		echo "return: $RTN, cmd: $line" >> $RST
	fi	
done

echo "TEST END" >> $RST
