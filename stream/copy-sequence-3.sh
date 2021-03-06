#!/bin/bash
echo "Acquire baseline"
logfile=`date +copy-%y-%m-%d-%R.log`
echo "start test (v3)" `date` >> $logfile
echo ./copy_progs -t -l $logfile
./copy_progs -t -p 1 -l $logfile
echo "Single emulated NVM run, multiple DRAM runs"
for i in {1..20}
do
   echo ./copy_progs -n /mnt/pmem0/copy-test -p 1 -s 23 -a $i -l $logfile
   ./copy_progs -n /mnt/pmem0/copy-test -p 1 -s 23 -a $i -l $logfile
done
echo "Single DRAM run, multiple emulated NVM runs"
for i in {1..20}
do
   echo ./copy_progs -f /mnt/pmem0/copy-test -p 1 -s 23 -a $i -l $logfile
   ./copy_progs -f /mnt/pmem0/copy-test -p 1 -s 23 -a $i -l $logfile
done
echo "Sanity: run DRAM in separate nodes"
echo ./copy_progs -p 1 -s 25 -l $logfile
./copy_progs -p 1 -s 25 -l $logfile
echo "Sanity: run DRAM in same node, single emulated NVM in separate node"
for i in {1..20}
do
   echo ./copy_progs -n /mnt/pmem0/copy-test -p 1 -s 25 -a $i -l $logfile
   ./copy_progs -n /mnt/pmem0/copy-test -p 1 -s 25 -a $i -l $logfile
done
echo "end test" `date` >> $logfile
