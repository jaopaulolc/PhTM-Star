#!/usr/bin/gnuplot

reset
set encoding utf8
set terminal eps enhanced color size 7.20, 5.40 font "NimbusSanL-Bold,16"

INFILE=system("echo $infile")
OUTFILE=system("echo $outfile").'.eps'
COLS=system("echo $cols")

set output OUTFILE

set style line 1 lt 1 lw 1.0 pt 7 ps 0.7 lc rgb "#66A61E"
set style line 2 lt 1 lw 1.0 pt 7 ps 0.7 lc rgb "#123455"
set style line 3 lt 1 lw 1.0 pt 7 ps 0.7 lc rgb "#1623DD"
set style line 4 lt 1 lw 1.0 pt 7 ps 0.7 lc rgb "#EA6507"
set style line 5 lt 1 lw 1.0 pt 7 ps 0.7 lc rgb "#D01E1E"
set style line 6 lt 1 lw 1.0 pt 7 ps 0.7 lc rgb "#E7298A"

set format y "%2.1tx10^%T"
set ytics add ('0' 0)

set notitle
set xlabel '"Time"'
set ylabel 'Throughput (transactions/s)'
set xtics nomirror
set ytics nomirror

#set key autotitle columnhead top outside horizontal right nobox font "NimbusSanL-Bold,26"
unset key
plot for[i=1:COLS] INFILE u 0:i ls i

set terminal pop
set output
