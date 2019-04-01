#!/bin/bash
logfile=`date +copy-v6-%y-%m-%d-%R.log`
echo "start test (v6)" `date` >> $logfile
echo "Acquire baseline"
echo ./copy_progs -t -l $logfile
./copy_progs -t -p 1 -l $logfile

echo "Mixed NVM runs, DRAM runs"
for i in {1..20}
do 
    echo ./copy_progs -f /mnt/pmem7/copy-test -p 25 -s 26 -a 1 -b $i -l $logfile
    ./copy_progs -f /mnt/pmem7/copy-test -p 25 -s 36 -a $1 -b $i -l $logfile
done
echo "Mixed eNVM runs, DRAM runs"
for i in {1..20}
do 
    echo ./copy_progs -f /mnt/pmem0/copy-test -p 1 -s 2 -a 1 -b $i -l $logfile
    ./copy_progs -f /mnt/pmem0/copy-test -p 1 -s 2 -a $1 -b $i -l $logfile
done
echo "end test" `date` >> $logfile
