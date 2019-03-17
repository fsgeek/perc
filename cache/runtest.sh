#!/bin/sh
#
# This runs the test program and collects data
#

# First, start by writing out the relevant system information
#
timestamp=`date '+%Y_%m_%d__%H_%M_%S'`;
hostname=`hostname`
BASE_FILE="./perc-test-"$hostname"-"$timestamp""
LOG_FILE="$BASE_FILE".log""
date > $LOG_FILE
echo "git log -1" >> $LOG_FILE
git log -1 >> $LOG_FILE
echo "uname -a" >> $LOG_FILE
uname -a >> $LOG_FILE
echo "lscpu" >> $LOG_FILE
lscpu >> $LOG_FILE
echo "cat /proc/meminfo" >> $LOG_FILE
cat /proc/meminfo >> $LOG_FILE
echo "dmidecode --type 17" >> $LOG_FILE
sudo dmidecode --type 17 >> $LOG_FILE
echo "lshw -quiet" >> $LOG_FILE
sudo lshw -quiet >> $LOG_FILE
echo "lsb_release -a" >> $LOG_FILE
lsb_release -a >> $LOG_FILE
echo "mount" >> $LOG_FILE
mount >> $LOG_FILE
if [ -f "/usr/bin/ndctl" ]; then
    echo "ndctl list --namespaces  --human" >> $LOG_FILE 2>&1
    ndctl list --namespaces  --human >> $LOG_FILE 2>&1
    echo "ndctl list -B -D -R -X" >> $LOG_FILE 2>&1
    ndctl list -B -D -R -X >> $LOG_FILE 2>&1
fi
if [ -f "/usr/bin/ipmctl" ]; then
       echo "ipmctl show -topology" >> $LOG_FILE 2>&1
       ipmctl show -topology >> $LOG_FILE 2>&1
fi
echo "****** TEST RESULTS *****" >> $LOG_FILE

#echo "./perc_progs" >> $LOG_FILE
#./perc_progs >> $BASE_FILE"-anonymous.json" 2>&1

#echo "./perc_progs -d /tmp/fsgeek-test.dat" >> $LOG_FILE
#./perc_progs -d /tmp/fsgeek-test.dat >> $BASE_FILE"-tmp.json" 2>&1

#echo "./perc_progs -d" `pwd`"/fsgeek-test.dat" >> $LOG_FILE
#./perc_progs -d (`whoami`)/fsgeek-test.dat >> $BASE_FILE"-home.json" 2>&1

#target=`pwd`/"fsgeek-test.dat"
#echo "./perc_progs -d" $target >> $LOG_FILE
#./perc_progs -d $target >> $BASE_FILE"-cwd.json" 2>&1

#if [ -d "/mnt/pmem0" ]; then
#    echo "./perc_progs -d /mnt/pmem0/fsgeek-test.dat" >> $LOG_FILE 
#    ./perc_progs -d /mnt/pmem0/fsgeek-test.dat >> $BASE_FILE"-emulated-pmem0.json" 2>&1
#fi

if [ -d "/mnt/pmem7" ]; then
    echo "./perc_progs -d /mnt/pmem7/fsgeek-test.dat" >> $LOG_FILE 
    ./perc_progs -d /mnt/pmem7/fsgeek-test.dat >> $BASE_FILE"-actual-pmem7.json" 2>&1
fi


