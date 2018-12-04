#!/bin/bash
logfile=`date +copy-v5-%y-%m-%d-%R.log`
echo "start test (v5)" `date` >> $logfile
echo "Acquire baseline"
echo ./copy_progs -t -l $logfile
./copy_progs -t -p 1 -l $logfile

echo "Mixed NVM runs, DRAM runs"
for i in {1..10}
do 
    for j in {1..10}
    do
        echo ./copy_progs -f /mnt/pmem1/copy-test -p 1 -s 13 -a $i -b $j -l $logfile
        ./copy_progs -f /mnt/pmem1/copy-test -p 1 -s 13 -a $i -b $j -l $logfile
    done
done
echo "Mixed eNVM runs, DRAM runs"
for i in {1..11}
do 
    for j in {1..11}
    do
        echo ./copy_progs -f /mnt/pmem0/copy-test -p 1 -s 13 -a $i -b $j -l $logfile
        ./copy_progs -f /mnt/pmem0/copy-test -p 1 -s 13 -a $i -b $j -l $logfile
    done
done
echo "end test" `date` >> $logfile
