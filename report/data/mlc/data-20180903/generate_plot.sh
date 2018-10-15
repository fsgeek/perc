#!/bin/bash

if [ $# -lt 1 ] ; then
   echo "Usage: generate_plot.sh \"outputfile.csv\""
   exit
fi

inputfile=$1
if [ ! -f ${inputfile} ] ; then
    echo "${inputfile} does not exist"
    exit
fi

sed '6!d' ${inputfile} | grep "write_count_max" > /dev/null
if [ $? -eq 0 ] ; then
    error_info=1
fi

short_format=1
sed '6!d' ${inputfile} | grep "bytes_read (derived)" > /dev/null
if [ $? -ne 0 ] ; then
    short_format=
    sed '8!d' ${inputfile} | grep "bytes_read (derived)" > /dev/null
    if [ $? -ne 0 -a -z "${error_info}" ] ; then
        echo "Unsupported format!"
        exit 1
    fi
    if [ -z "${error_info}" ] ; then
        metric_grouping_format=1
        echo "Metric grouping format (-g option output) is not supported currently! Please use the default format ..."
        exit 1
    fi
fi

if [ -n "${error_info}" ] ; then
    declare -a metric_list=('write_count_max' 'write_count_avg' 'uncorrectable_host' 'uncorrectable_non_host' 'media_errors_uc' 'media_errors_ce' 'media_errors_ecc' 'dram_errors_uc' 'dram_errors_ce')
    TOTAL_NUM_METRIC=9
else
    declare -a metric_list=('bytes_read (derived)' 'bytes_written (derived)' 'read_hit_ratio (derived)' 'write_hit_ratio (derived)' 'wdb_merge_percent (derived)' 'sxp_read_ops (derived)' 'sxp_write_ops (derived)' 'read_64B_ops_received' 'write_64B_ops_received' 'ddrt_read_ops' 'ddrt_write_ops')
    TOTAL_NUM_METRIC=13
fi

METRIC_OFFSET=3

metric_list_size=${#metric_list[@]}
#echo ${metric_list[@]}
#echo ${metric_list_size}

# remove extension from input file to create gnuplot script filename
gnuplotfile=`expr "${inputfile}" | rev | cut -d "." -f 2- | rev`
if [[ ${gnuplotfile} =~ ^[./]+$ ]] ; then
    pngoutfile=${inputfile}.png
    gnuplotfile=${inputfile}.sh
else
    pngoutfile=${gnuplotfile}.png
    gnuplotfile=${gnuplotfile}.sh
fi

if [ -n "${metric_grouping_format}" ] ; then
    dimm_line_num=7
else
    dimm_line_num=5
fi
num_dimms=$( head -${dimm_line_num} ${inputfile} | grep -o DIMM | wc -w )

echo "#!/usr/bin/gnuplot"             > ${gnuplotfile}
#echo set terminal wxt persist         >> ${gnuplotfile}
#echo set terminal wxt size 1500,1000  >> ${gnuplotfile}
# echo set terminal png			>> ${gnuplotfile}
echo set terminal png size 1500, 1000 >> ${gnuplotfile}
echo set output \"${pngoutfile}\"	>> ${gnuplotfile}
echo set multiplot layout 6, 2        >> ${gnuplotfile}
echo set xdata time                   >> ${gnuplotfile}
echo set autoscale y                  >> ${gnuplotfile}
echo file=\"${inputfile}\"            >> ${gnuplotfile}
echo set timefmt \"%s\"               >> ${gnuplotfile}
echo set datafile separator \"\;\"    >> ${gnuplotfile}
echo set xtics axis rangelimited      >> ${gnuplotfile}
echo set xtics scale 0.5 rotate by 25 offset -3,-0.5 >> ${gnuplotfile}

itr=0
while [ ${itr} -lt ${metric_list_size} ]
do
    metric=${metric_list[itr]}

    # replace "_" with space for plot title
    metric_title="${metric//_/ }"

    if [ ${itr} -eq `expr ${metric_list_size} - 2` ] ; then
        echo set xlabel \"time\"              >> ${gnuplotfile}
    fi
    if [[ $itr -eq 2 && ${error_info} == "0" ]] ; then
        echo set yrange[0:1]                  >> ${gnuplotfile}
    fi
    if [[ $itr -eq 5 && ${error_info} == "0" ]] ; then
        echo set autoscale y                  >> ${gnuplotfile}
    fi
    echo set title \"${metric_title}\"        >> ${gnuplotfile}
    echo plot \\                              >> ${gnuplotfile}
    iter=0
    offset=`expr ${METRIC_OFFSET} + ${itr}`
    while [ ${iter} -lt `expr ${num_dimms} - 1` ] ; do
        echo file using 1:${offset} every ::6 title \"DIMM `expr ${iter} + 1`\" with lines, \\ >> ${gnuplotfile}
        iter=`expr ${iter} + 1`
        offset=`expr ${offset} + ${TOTAL_NUM_METRIC}`
    done
    echo file using 1:${offset} every ::6 title \"DIMM `expr ${iter} + 1`\" with lines             >> ${gnuplotfile}

    if [[ ${itr} -eq `expr ${metric_list_size} - 1` ]] ; then
        echo unset multiplot                  >> ${gnuplotfile}
        chmod u+x ${gnuplotfile}
    fi

    echo Processed metric ${metric}.

    itr=`expr ${itr} + 1`
done

echo "gnuplot script ${gnuplotfile} created."
chmod u+x ${gnuplotfile}
if [ $? -ne 0 ] ; then
    echo "${gnuplotfile}: Change mode to executable failed!"\
    exit
fi

if [ -z $DO_NOT_DISPLAY ] ; then
    ./${gnuplotfile}
fi
