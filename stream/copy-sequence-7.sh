#!/bin/bash
logfile=`date +copy-v7-%y-%m-%d-%R.log`
datafile=`date +copy-v7-%y-%m-%d-%R.dat`
iterations=10
echo "start test (v7)" `date` >> $logfile
echo "Acquire baseline" >> $logfile
echo ./copy_progs -t -l $datafile -i $iterations >> $logfile
#./copy_progs -t -p 1 -l $datafile -i $iterations >> $logfile

echo "Mixed NVM runs, DRAM runs" >> $logfile
for i in {1..20}
do 
    echo ./copy_progs -f /mnt/pmem7/copy-test -p 25 -s 26 -a 1 -b $i -i $iterations -l $datafile  >> $logfile
#    ./copy_progs -f /mnt/pmem7/copy-test -p 25 -s 26 -a $1 -b $i -i $iterations -l $datafile >> $logfile
done
echo "Mixed eNVM runs, DRAM runs"
for i in {1..20}
do 
    echo ./copy_progs -f /mnt/pmem0/copy-test -p 1 -s 2 -a 1 -b $i -i $iterations -l $datafile >> $logfile
#    ./copy_progs -f /mnt/pmem0/copy-test -p 1 -s 2 -a $1 -b $i -i $iterations -l $datafile >> $logfile
done
echo "end test" `date` >> $logfile
