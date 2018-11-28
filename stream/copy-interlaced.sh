#!/bin/sh
 
# First, let's do some DRAM baselines
echo "Acquire baseline"
logfile=`date +copy-interlaced-%y-%m-%d-%R.log`
echo ./copy_progs -t -l $logfile
./copy_progs -t -l $logfile -p 17
# This tests interleaved behavior with just DRAM

echo "Interleaved memory with DRAM"
echo ./copy_progs -v -l $logfile -p 17
./copy_progs -v -l $logfile -p 17

echo "NVM primary, DRAM secondary (pmem3)"
echo ./copy_progs -v -f /mnt/pmem3/wam-test1.dat -p 17 -l $logfile
./copy_progs -v -f /mnt/pmem3/wam-test1.dat -p 17 -l $logfile

echo "NVM primary, DRAM secondary (pmem0)"
echo ./copy_progs -v -f /mnt/pmem0/wam-test1.dat -p 17 -l $logfile
./copy_progs -v -f /mnt/pmem0/wam-test1.dat -p 17 -l $logfile

echo "DRAM primary, NVM secondary (pmem3)"
echo ./copy_progs -v -n /mnt/pmem3/wam-test1.dat -p 17 -l $logfile
./copy_progs -v -n /mnt/pmem3/wam-test1.dat -p 17 -l $logfile

echo "DRAM primary, NVM secondary (pmem0)"
echo ./copy_progs -v -n /mnt/pmem0/wam-test1.dat -p 17 -l $logfile
./copy_progs -v -n /mnt/pmem0/wam-test1.dat -p 17 -l $logfile

echo "NVM primary, NVM secondary (pmem3)"
echo ./copy_progs -v -f /mnt/pmem3/wam-test1.dat -n /mnt/pmem3/wam-test2.dat -p 17 -l $logfile
./copy_progs -v -f /mnt/pmem3/wam-test1.dat -n /mnt/pmem3/wam-test2.dat -p 17 -l $logfile

echo "NVM primary, NVM secondary (pmem3) - sanity check (reversed files)"
echo ./copy_progs -v -n /mnt/pmem3/wam-test1.dat -f /mnt/pmem3/wam-test2.dat -p 17 -l $logfile
./copy_progs -v -n /mnt/pmem3/wam-test1.dat -f /mnt/pmem3/wam-test2.dat -p 17 -l $logfile

echo "NVM primary (pmem0), NVM secondary (pmem3)"
echo ./copy_progs -v -f /mnt/pmem0/wam-test1.dat -n /mnt/pmem3/wam-test2.dat -p 17 -l $logfile
./copy_progs -v -f /mnt/pmem0/wam-test1.dat -n /mnt/pmem3/wam-test2.dat -p 17 -l $logfile

echo "NVM primary (pmem3), NVM secondary (pmem0) - sanity check (reversed files)"
echo ./copy_progs -v -n /mnt/pmem0/wam-test1.dat -f /mnt/pmem3/wam-test2.dat -p 17 -l $logfile
./copy_progs -v -n /mnt/pmem0/wam-test1.dat -f /mnt/pmem3/wam-test2.dat -p 17 -l $logfile

