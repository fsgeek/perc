#!/bin/bash
echo "Acquire baseline"
logfile=`date +copy-%y-%m-%d-%R.log`
echo ./copy_progs -t -l $logfile
./copy_progs -t -l $logfile
for i in {1..20}
do
   echo ./copy_progs -f /mnt/pmem0/copy-test -p 1 -s 20 -a $i -l $logfile
   ./copy_progs -f /mnt/pmem0/copy-test -p 1 -s 23 -a $i -l $logfile
done
