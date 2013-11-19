#!/bin/bash

gnuplot=/usr/bin/gnuplot

function plot-STAMP-HLE-RTM-tinySTM {
	rm -f {tsx-hle,tsx-rtm,tinySTM,seq}.speedup
	for app in $APPS; do
		eval $app=$(sed -n '2,$'p $output/$app-seq.time | cut -d' ' -f1)
		sed -n '2,$'p $output/$app-tsx-hle.time | awk '{print '${!app}'/$1," ",$2}' >> tsx-hle.speedup
		sed -n '2,$'p $output/$app-tsx-rtm.time | awk '{print '${!app}'/$1," ",$2}' >> tsx-rtm.speedup
		sed -n '2,$'p $output/$app-ETL-SUICIDE.time | awk '{print '${!app}'/$1," ",$2}' >> tinySTM.speedup
		echo >> tsx-hle.speedup
		echo >> tsx-rtm.speedup
		echo >> tinySTM.speedup
	done
	test -e $gnuplot && $gnuplot <<-EOF
		set encoding utf8
		set terminal postscript eps enhanced color size 9.60,2.80 font "Helvetic-Bold,16"
		set output '$output/stamp-hle-rtm-tinySTM.eps'
	
		# estilo das linhas
		set style line 1 lt 1 lw 5.0 pt 7 ps 1.0 lc rgb "black"
		set style line 2 lt 1 lw 5.0 pt 7 ps 1.0 lc rgb "#8C1717" #scarlet
		set style line 3 lt 1 lw 5.0 pt 7 ps 1.0 lc rgb "#EE7621" #chocolate2
		set style line 4 lt 1 lw 5.0 pt 7 ps 1.0 lc rgb "#99CC32" #yellowgreen
		
		set xtics 0,5,10 nomirror format ""
		set xtics add 5 out scale 6,2
		set mxtics 5
		set ytics out nomirror
		set grid y
	
		set ylabel "Speedup (relação ao sequencial)" offset 1.8
		
		ncores = "| $NB_CORES"
		apps = "$APPS"
		set for[i=1:words(ncores)*words(apps)] label word(ncores,i%5 + 1) at i,-0.40 center
		set for[i=1:words(apps)] label word(apps,i) at (2*i-1)*2.5,-0.75 center

		plot "tsx-hle.speedup" u (column(0) + 1 + int(column(0)/4)):1 w lp ls 2 t 'HLE', \
				 "tsx-rtm.speedup" u (column(0) + 1 + int(column(0)/4)):1 w lp ls 3 t 'RTM', \
				 "tinySTM.speedup" u (column(0) + 1 + int(column(0)/4)):1 w lp ls 4 t 'tinySTM'
		
		set terminal ""
		set output
	EOF
	rm -f {tsx-hle,tsx-rtm,tinySTM,seq}.speedup
}

function plot-HLE-RTM-tinySTM {
	for app in $APPS; do
		#rm -f $app-seq.time *.temp
		#echo -n $NB_CORES | sed 's| |\n|g' > ncores.temp
		#for i in $NB_CORES; do
		#	cat $output/$app-seq.time >> $app-seq.temp
		#done
		#paste ncores.temp $app-seq.temp > $app-seq.time
		test -e $gnuplot && $gnuplot <<-EOF
			set encoding utf8
			set terminal postscript eps enhanced color size 7.20,2.40 font "Helvetic-Bold,16"
			set output '$output/$app-hle-rtm-tinySTM.eps'

			set multiplot layout 1,3 title "${app^}"
			set tmargin 1.2
			set lmargin 6.5
			set rmargin 0


			# estilos das linhas
			set style line 1 lt 1 lw 5.0 pt 7 ps 1.8 lc rgb "black"
			set style line 2 lt 1 lw 5.0 pt 7 ps 1.8 lc rgb "#8C1717" #scarlet
			set style line 3 lt 1 lw 5.0 pt 7 ps 1.8 lc rgb "#EE7621" #chocolate2
			set style line 4 lt 1 lw 5.0 pt 7 ps 1.8 lc rgb "#99CC32" #yellowgreen
			
			set xtics out nomirror
			set ytics out nomirror
			set grid y
			unset xlabel

			# HLE plot
			set x2label "HLE"
			set ylabel "Tempo de Execução (s)" offset 2.2
			set size 0.335,0.9
			set key top left

			set xtics ('1' 1 -1,'2' 2 -1,'4' 3 -1,'8' 4 -1)
	
			plot for[i=1:8:2] "$output/$app-tsx-hle.time" u 0:i w lp ls (i+1)/2 pt i t columnhead((i+1)/2)
			
			unset key
			unset ylabel

			# RTM plot
			set x2label "RTM"
			set size 0.33,0.9

			plot for[i=1:8:2] "$output/$app-tsx-rtm.time" u 0:i w lp ls (i+1)/2 pt i t columnhead((i+1)/2)
			
			set lmargin 5.5
			set rmargin 0.8
			# tinySTM plot
			set x2label "tinySTM"
			set size 0.33,0.9

			plot for[i=1:8:2] "$output/$app-ETL-SUICIDE.time" u 0:i w lp ls (i+1)/2 pt i t columnhead((i+1)/2)
			#		 "$app-seq.time"             u (column(0)+1):2:xtic(1) w lp ls 1 t 'SEQ'

			unset multiplot
			
			set terminal ""
			set output
		EOF
		rm -f $app-seq.time *.temp
	done
}
