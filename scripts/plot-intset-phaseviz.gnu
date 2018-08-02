#!/usr/bin/gnuplot

reset
set encoding utf8
set terminal pdfcairo enhanced color size 7.20, 5.40 font "NimbusSanL-Bold,16"

TITLE=system("echo $title")
INFILE=system("echo $infile")
OUTFILE=system("echo $outfile").'.pdf'

set output OUTFILE

set style line 1 lt 1 lw 1.0 pt 7 ps 0.7 lc rgb "#66A61E"
set style line 2 lt 1 lw 1.0 pt 7 ps 0.7 lc rgb "#123455"
set style line 3 lt 1 lw 1.0 pt 7 ps 0.7 lc rgb "#1623DD"
set style line 4 lt 1 lw 1.0 pt 7 ps 0.7 lc rgb "#EA6507"
set style line 5 lt 1 lw 1.0 pt 7 ps 0.7 lc rgb "#D01E1E"
set style line 6 lt 1 lw 1.0 pt 7 ps 0.7 lc rgb "#E7298A"

bm = 0.05
lm = 0.14
rm = 0.97
tm = 0.5
gap = 0.03
size = 1
y1 = system("echo $y1")
y2 = system("echo $y2")
y3 = system("echo $y3")
y4 = system("echo $y4")

#y1 = y1 - 5000
#y2 = y2 + 5000
#y3 = y3 - 5000
#y4 = y4 + 5000

set multiplot
set xlabel '"Time"'
set ylabel 'Throughput (transactions/s)' offset 0, 7
set border 1+2+8
set xtics nomirror
set ytics nomirror
set lmargin at screen lm
set rmargin at screen rm

#set logscale y
#unset logscale x

set key autotitle columnhead center right nobox font "NimbusSanL-Bold,26"
set bmargin at screen bm + 0.04
set tmargin at screen tm
set yrange [y1:y2]
#plot [0:4000] for[i=1:5] INFILE u 0:(column(0) < 1900 ? column(i) : 1/0) ls i
plot [0:2000] for[i=1:5] INFILE u 0:i ls i

set title TITLE offset 0, -0.8
unset key
unset xtics
unset xlabel
unset ylabel
set border 2+4+8
set bmargin at screen tm + gap
set tmargin at screen 1 - bm
set yrange [y3:y4]
#plot [0:4000] for[i=1:5] INFILE u 0:(column(0) > 2100 ? column(i) : 1/0) ls i
plot [0:2000] for[i=1:5] INFILE u 0:i ls i
unset multiplot

set terminal pop
set output
