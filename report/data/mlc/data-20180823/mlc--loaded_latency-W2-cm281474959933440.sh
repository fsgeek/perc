#!/usr/bin/gnuplot
set terminal wxt persist
set terminal wxt size 1500,1000
set multiplot layout 6, 2
set xdata time
set autoscale y
file="mlc--loaded_latency-W2-cm281474959933440.csv"
set timefmt "%s"
set datafile separator ";"
set xtics axis rangelimited
set xtics scale 0.5 rotate by 25 offset -3,-0.5
set title "bytes read (derived)"
plot \
file using 1:3 every ::6 title "DIMM 1" with lines, \
file using 1:16 every ::6 title "DIMM 2" with lines, \
file using 1:29 every ::6 title "DIMM 3" with lines, \
file using 1:42 every ::6 title "DIMM 4" with lines, \
file using 1:55 every ::6 title "DIMM 5" with lines, \
file using 1:68 every ::6 title "DIMM 6" with lines, \
file using 1:81 every ::6 title "DIMM 7" with lines, \
file using 1:94 every ::6 title "DIMM 8" with lines, \
file using 1:107 every ::6 title "DIMM 9" with lines, \
file using 1:120 every ::6 title "DIMM 10" with lines, \
file using 1:133 every ::6 title "DIMM 11" with lines, \
file using 1:146 every ::6 title "DIMM 12" with lines
set title "bytes written (derived)"
plot \
file using 1:4 every ::6 title "DIMM 1" with lines, \
file using 1:17 every ::6 title "DIMM 2" with lines, \
file using 1:30 every ::6 title "DIMM 3" with lines, \
file using 1:43 every ::6 title "DIMM 4" with lines, \
file using 1:56 every ::6 title "DIMM 5" with lines, \
file using 1:69 every ::6 title "DIMM 6" with lines, \
file using 1:82 every ::6 title "DIMM 7" with lines, \
file using 1:95 every ::6 title "DIMM 8" with lines, \
file using 1:108 every ::6 title "DIMM 9" with lines, \
file using 1:121 every ::6 title "DIMM 10" with lines, \
file using 1:134 every ::6 title "DIMM 11" with lines, \
file using 1:147 every ::6 title "DIMM 12" with lines
set title "read hit ratio (derived)"
plot \
file using 1:5 every ::6 title "DIMM 1" with lines, \
file using 1:18 every ::6 title "DIMM 2" with lines, \
file using 1:31 every ::6 title "DIMM 3" with lines, \
file using 1:44 every ::6 title "DIMM 4" with lines, \
file using 1:57 every ::6 title "DIMM 5" with lines, \
file using 1:70 every ::6 title "DIMM 6" with lines, \
file using 1:83 every ::6 title "DIMM 7" with lines, \
file using 1:96 every ::6 title "DIMM 8" with lines, \
file using 1:109 every ::6 title "DIMM 9" with lines, \
file using 1:122 every ::6 title "DIMM 10" with lines, \
file using 1:135 every ::6 title "DIMM 11" with lines, \
file using 1:148 every ::6 title "DIMM 12" with lines
set title "write hit ratio (derived)"
plot \
file using 1:6 every ::6 title "DIMM 1" with lines, \
file using 1:19 every ::6 title "DIMM 2" with lines, \
file using 1:32 every ::6 title "DIMM 3" with lines, \
file using 1:45 every ::6 title "DIMM 4" with lines, \
file using 1:58 every ::6 title "DIMM 5" with lines, \
file using 1:71 every ::6 title "DIMM 6" with lines, \
file using 1:84 every ::6 title "DIMM 7" with lines, \
file using 1:97 every ::6 title "DIMM 8" with lines, \
file using 1:110 every ::6 title "DIMM 9" with lines, \
file using 1:123 every ::6 title "DIMM 10" with lines, \
file using 1:136 every ::6 title "DIMM 11" with lines, \
file using 1:149 every ::6 title "DIMM 12" with lines
set title "wdb merge percent (derived)"
plot \
file using 1:7 every ::6 title "DIMM 1" with lines, \
file using 1:20 every ::6 title "DIMM 2" with lines, \
file using 1:33 every ::6 title "DIMM 3" with lines, \
file using 1:46 every ::6 title "DIMM 4" with lines, \
file using 1:59 every ::6 title "DIMM 5" with lines, \
file using 1:72 every ::6 title "DIMM 6" with lines, \
file using 1:85 every ::6 title "DIMM 7" with lines, \
file using 1:98 every ::6 title "DIMM 8" with lines, \
file using 1:111 every ::6 title "DIMM 9" with lines, \
file using 1:124 every ::6 title "DIMM 10" with lines, \
file using 1:137 every ::6 title "DIMM 11" with lines, \
file using 1:150 every ::6 title "DIMM 12" with lines
set title "sxp read ops (derived)"
plot \
file using 1:8 every ::6 title "DIMM 1" with lines, \
file using 1:21 every ::6 title "DIMM 2" with lines, \
file using 1:34 every ::6 title "DIMM 3" with lines, \
file using 1:47 every ::6 title "DIMM 4" with lines, \
file using 1:60 every ::6 title "DIMM 5" with lines, \
file using 1:73 every ::6 title "DIMM 6" with lines, \
file using 1:86 every ::6 title "DIMM 7" with lines, \
file using 1:99 every ::6 title "DIMM 8" with lines, \
file using 1:112 every ::6 title "DIMM 9" with lines, \
file using 1:125 every ::6 title "DIMM 10" with lines, \
file using 1:138 every ::6 title "DIMM 11" with lines, \
file using 1:151 every ::6 title "DIMM 12" with lines
set title "sxp write ops (derived)"
plot \
file using 1:9 every ::6 title "DIMM 1" with lines, \
file using 1:22 every ::6 title "DIMM 2" with lines, \
file using 1:35 every ::6 title "DIMM 3" with lines, \
file using 1:48 every ::6 title "DIMM 4" with lines, \
file using 1:61 every ::6 title "DIMM 5" with lines, \
file using 1:74 every ::6 title "DIMM 6" with lines, \
file using 1:87 every ::6 title "DIMM 7" with lines, \
file using 1:100 every ::6 title "DIMM 8" with lines, \
file using 1:113 every ::6 title "DIMM 9" with lines, \
file using 1:126 every ::6 title "DIMM 10" with lines, \
file using 1:139 every ::6 title "DIMM 11" with lines, \
file using 1:152 every ::6 title "DIMM 12" with lines
set title "read 64B ops received"
plot \
file using 1:10 every ::6 title "DIMM 1" with lines, \
file using 1:23 every ::6 title "DIMM 2" with lines, \
file using 1:36 every ::6 title "DIMM 3" with lines, \
file using 1:49 every ::6 title "DIMM 4" with lines, \
file using 1:62 every ::6 title "DIMM 5" with lines, \
file using 1:75 every ::6 title "DIMM 6" with lines, \
file using 1:88 every ::6 title "DIMM 7" with lines, \
file using 1:101 every ::6 title "DIMM 8" with lines, \
file using 1:114 every ::6 title "DIMM 9" with lines, \
file using 1:127 every ::6 title "DIMM 10" with lines, \
file using 1:140 every ::6 title "DIMM 11" with lines, \
file using 1:153 every ::6 title "DIMM 12" with lines
set title "write 64B ops received"
plot \
file using 1:11 every ::6 title "DIMM 1" with lines, \
file using 1:24 every ::6 title "DIMM 2" with lines, \
file using 1:37 every ::6 title "DIMM 3" with lines, \
file using 1:50 every ::6 title "DIMM 4" with lines, \
file using 1:63 every ::6 title "DIMM 5" with lines, \
file using 1:76 every ::6 title "DIMM 6" with lines, \
file using 1:89 every ::6 title "DIMM 7" with lines, \
file using 1:102 every ::6 title "DIMM 8" with lines, \
file using 1:115 every ::6 title "DIMM 9" with lines, \
file using 1:128 every ::6 title "DIMM 10" with lines, \
file using 1:141 every ::6 title "DIMM 11" with lines, \
file using 1:154 every ::6 title "DIMM 12" with lines
set xlabel "time"
set title "ddrt read ops"
plot \
file using 1:12 every ::6 title "DIMM 1" with lines, \
file using 1:25 every ::6 title "DIMM 2" with lines, \
file using 1:38 every ::6 title "DIMM 3" with lines, \
file using 1:51 every ::6 title "DIMM 4" with lines, \
file using 1:64 every ::6 title "DIMM 5" with lines, \
file using 1:77 every ::6 title "DIMM 6" with lines, \
file using 1:90 every ::6 title "DIMM 7" with lines, \
file using 1:103 every ::6 title "DIMM 8" with lines, \
file using 1:116 every ::6 title "DIMM 9" with lines, \
file using 1:129 every ::6 title "DIMM 10" with lines, \
file using 1:142 every ::6 title "DIMM 11" with lines, \
file using 1:155 every ::6 title "DIMM 12" with lines
set title "ddrt write ops"
plot \
file using 1:13 every ::6 title "DIMM 1" with lines, \
file using 1:26 every ::6 title "DIMM 2" with lines, \
file using 1:39 every ::6 title "DIMM 3" with lines, \
file using 1:52 every ::6 title "DIMM 4" with lines, \
file using 1:65 every ::6 title "DIMM 5" with lines, \
file using 1:78 every ::6 title "DIMM 6" with lines, \
file using 1:91 every ::6 title "DIMM 7" with lines, \
file using 1:104 every ::6 title "DIMM 8" with lines, \
file using 1:117 every ::6 title "DIMM 9" with lines, \
file using 1:130 every ::6 title "DIMM 10" with lines, \
file using 1:143 every ::6 title "DIMM 11" with lines, \
file using 1:156 every ::6 title "DIMM 12" with lines
unset multiplot
