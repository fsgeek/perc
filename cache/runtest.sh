#!/bin/sh
#
# This runs the test program and collects data
#

# First, start by writing out the relevant system information
#
LOG_FILE="./perc-test.log"

date > $LOG_FILE
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
echo "./perc_progs" >> $LOG_FILE
./perc_progs >> $LOG_FILE 2>&1
echo "./perc_progs -d /tmp/fsgeek-test.dat" >> $LOG_FILE
./perc_progs -d /tmp/fsgeek-test.dat >> $LOG_FILE 2>&1
if [ -d "/mnt/pmem0p1" ]; then
    echo "./perc_progs -d /mnt/pmem0p1/fsgeek-test.dat" >> $LOG_FILE
    ./perc_progs -d /mnt/pmem0p1/fsgeek-test.dat >> $LOG_FILE
fi


