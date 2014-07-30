#!/bin/bash

output=output-data
MAKE_OPTIONS='--quiet --no-keep-going'
APPS='bayes genome intruder kmeans labyrinth ssca2 vacation yada'
DESIGNS='ETL CTL WT'
CMS='SUICIDE DELAY BACKOFF'
RTMCMS='spinlock1 spinlock2 auxlock backoff trylock'
BUILDS='tinystm seq lock tsx-hle tsx-rtm'
MEMALLOCS='ptmalloc tcmalloc hoard tbbmalloc'
GOVERNORS='powersave conservative ondemand userspace performance'
userspace="$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies | awk '{print $((NF+1)/2)}')"
ALLOCS_DIR='allocators'
ptmalloc=''
tcmalloc="$ALLOCS_DIR/libtcmalloc_minimal.so.4.1.0"
hoard="$ALLOCS_DIR/libhoard.so"
tbbmalloc="$ALLOCS_DIR/libtbbmalloc.so.2"
spinlock1='# DEFINES += -DRTM_CM_SPINLOCK1'
spinlock2='# DEFINES += -DRTM_CM_SPINLOCK2'
auxlock='# DEFINES += -DRTM_CM_AUXLOCK'
backoff='# DEFINES += -DRTM_CM_BACKOFF'
trylock='# DEFINES += -DRTM_CM_TRYLOCK'
ETL='# DEFINES += -DDESIGN=WRITE_BACK_ETL'
CTL='# DEFINES += -DDESIGN=WRITE_BACK_CTL'
WT='# DEFINES += -DDESIGN=WRITE_THROUGH'
SUICIDE='# DEFINES += -DCM=CM_SUICIDE'
DELAY='# DEFINES += -DCM=CM_DELAY'
BACKOFF='# DEFINES += -DCM=CM_BACKOFF'
GET_TIME="grep -e '^Time[ ]\+= ' data.temp | awk '{print \$3}' "
GET_ENERGY="grep -e '^Energy[ ]\+=' data.temp | awk '{print \$3}' "
GET_ABORT='grep -e "^[ ]\+\*" data.temp | awk "$parseAbort" '
EXEC_FLAG_bayes='-v32 -r4096 -n10 -p40 -i2 -e8 -s1 -t'
EXEC_FLAG_genome='-g16384 -s64 -n16777216 -t'
EXEC_FLAG_intruder='-a10 -l128 -n262144 -s1 -t'
EXEC_FLAG_kmeans='-m15 -n15 -T0.00001 -i stamp/data/kmeans/inputs/random-n65536-d32-c16.txt -t'
EXEC_FLAG_labyrinth='-i stamp/data/labyrinth/inputs/random-x512-y512-z7-n512.txt -t'
EXEC_FLAG_ssca2='-s20 -i1.0 -u1.0 -l3 -p3 -t'
EXEC_FLAG_vacation='-n4 -q60 -u90 -r1048576 -T4194304 -t'
EXEC_FLAG_yada='-a15 -i stamp/data/yada/inputs/ttimeu1000000.2 -t'
PCM_PATH=IntelPCM/
_AUX='$(echo ${build/tsx-/} | tr "[:lower:]" "[:upper:]")'
PCM_FLAGS='-e ${_AUX}_RETIRED.ABORTED_MISC1 -e ${_AUX}_RETIRED.ABORTED_MISC2
					 -e ${_AUX}_RETIRED.ABORTED_MISC3 -e ${_AUX}_RETIRED.ABORTED_MISC4'

awkscript='
BEGIN{
	j=0;
	for(i=1; i <= NF; i++){
		sum[i] = 0;
	}
}
{
	for(i=1; i <= NF; i++){
		sum[i] += $i;
		v[j,i] = $i;
	}
	j++
}
END{
	
	for(j=1; j <= NF;j++){
		mean=sum[j]/NR;
		dp=0;
		for(i=0;i<NR;i++)
			dp+=(mean-v[i,j])*(mean-v[i,j]);
		dp=dp/(NR-1);
		dp=sqrt(dp);
		printf "%.3lf %.3lf ", mean ,0.98*(dp/sqrt(NR));
	}
	print ""
}'

parseAbort='
BEGIN{
	M["K"] = 1000;
	M["M"] = 1000*M["K"];
	M["G"] = 1000*M["M"];
	M["T"] = 1000*M["G"];
}
{
	for(i=2; i <= NF;i++){
		if(M[$(i+1)]){
			printf "%ld " , $i*M[$(i+1)]
			i++
		}
		else{
			printf "%ld " , $i
		}
	}
}
END{
	print ""
}'

function usage {
	
	echo $0' (comp | exec | plot | clean | help) [-d <tinySTM DESIGN>]  [-m <tinySTM CM>] [-a <stamp app>]'
	echo -e '\t[-t <nb cores>] [-n <nb executions>] [-b <build>] [-p <PLOT>] [-M <memalloc>] [-g <governor>]'

	echo
	echo 'comp: compile.'
	echo 'exec: execute.'
	echo 'plot: plot available statistics.'
	echo 'clean: remove all executable, object and library files from all directories'
	echo 'help: show this help.'

	echo
	echo '-d <tinySTM DESIGN>'
	echo 'ETL | CTL | WT'
	echo 'default = ALL'

	echo
	echo '-m <tinySTM CM>'
	echo 'SUICIDE | BACKOFF | DELAY'
	echo 'default = ALL'
	
	echo
	echo '-D <RTM CM>'
	echo 'spinlock1 | spinlock2 | auxlock | backoff | trylock'
	echo 'default = ALL'

	echo
	echo '-a <stamp app>'
	echo 'bayes | genome | intruder | kmeans | labyrinth | ssca2 | vacation | yada'
	echo 'default = ALL'

	echo
	echo '-b <build>'
	echo 'tinystm | seq | lock | tsx-hle | tsx-rtm'
	echo 'default = ALL'
	
	echo
	echo '-M <memalloc>'
	echo 'ptmalloc | tcmalloc | hoard | tbbmalloc'
	echo 'default = ptmalloc'
	
	echo
	echo '-g <governor>'
	echo 'powersave | conservative | ondemand | userspace | performance'
	echo 'default = ondemand'
	
	echo
	echo '-p <PLOT>'
	echo 'selects which plot to performe'
	echo 'default = (none)'

	echo
	echo '-t <nb cores>'
	echo '1 | 2 | 4 | etc... | "1 2 4 8" | "4 8 16" | etc...'
	echo 'default = "1 2 4 8"'

	echo
	echo '-n <nb executions>'
	echo 'default = 20'
}

function compile {

	make -C msr/ clean $MAKE_OPTIONS && make -C msr/ $MAKE_OPTIONS
	
	echo 'starting compilation...'

	test ! -z "$STM_DESIGNS" && DESIGNS=$STM_DESIGNS
	test ! -z "$STM_CMS"     && CMS=$STM_CMS
	test ! -z "$STAMP_APP"   && APPS=$STAMP_APP
	test ! -z "$RTM_CMS"     && RTMCMS=$RTM_CMS

	echo
	echo -n "compiling intel PCM..."
	(make -C $PCM_PATH clean ${MAKE_OPTIONS} 2>&1 /dev/null && make -C $PCM_PATH pcm-tsx.x ${MAKE_OPTIONS} 2>&1 > /dev/null) 2>&1 > /dev/null
	echo "done."

	for build in $BUILDS; do
		if [ $build == "tinystm" ]; then
			for design in ${DESIGNS}; do
				for cm in ${CMS}; do
					sed "/${!design}/s|# ||;/${!cm}/s|# ||" tinySTM/Makefile.template > tinySTM/Makefile
					touch tinySTM/Makefile
					make -C tinySTM/ clean ${MAKE_OPTIONS}
					make -C tinySTM/ ${MAKE_OPTIONS}
					
					echo "## tinySTM ${design}-${cm} compiled"
					
					for app in ${APPS}; do
						SRC_APP_PATH=stamp/apps/$app
						BIN_APP_PATH=stamp/tinystm/$app
						make -C $SRC_APP_PATH -f Makefile TMBUILD=tinystm clean ${MAKE_OPTIONS} 2> /dev/null
						make -C $SRC_APP_PATH -f Makefile TMBUILD=tinystm ${MAKE_OPTIONS} 2> /dev/null
						mv $BIN_APP_PATH/$app $BIN_APP_PATH/${app}-${design}-${cm}
						echo "## ${app}-${design}-${cm} compiled"
					done # FOR EACH APP
				done # FOR EACH CM
			done # FOR EACH DESIGN
		else
			if [ $build == "tsx-rtm" ]; then
				for cm in ${RTMCMS}; do
					sed "/${!cm}/s|# ||" tsx/rtm/Makefile.template > tsx/rtm/Makefile
					touch tsx/rtm/Makefile
					make -C tsx/rtm clean ${MAKE_OPTIONS}
					make -C tsx/rtm ${MAKE_OPTIONS}
					for app in ${APPS}; do
						SRC_APP_PATH=stamp/apps/$app
						BIN_APP_PATH=stamp/$build/$app
						make -C $SRC_APP_PATH -f Makefile TMBUILD=$build clean ${MAKE_OPTIONS} 2> /dev/null
						make -C $SRC_APP_PATH -f Makefile TMBUILD=$build ${MAKE_OPTIONS} 2> /dev/null
						mv $BIN_APP_PATH/$app $BIN_APP_PATH/${app}-${build}-${cm}
						echo "## ${app}-$build-$cm compiled"
					done # FOR EACH APP
					rm -f tsx/rtm/Makefile
				done
			else
				for app in ${APPS}; do
					SRC_APP_PATH=stamp/apps/$app
					BIN_APP_PATH=stamp/$build/$app
					make -C $SRC_APP_PATH -f Makefile TMBUILD=$build clean ${MAKE_OPTIONS} 2> /dev/null
					make -C $SRC_APP_PATH -f Makefile TMBUILD=$build ${MAKE_OPTIONS} 2> /dev/null
					mv $BIN_APP_PATH/$app $BIN_APP_PATH/${app}-${build}
					echo "## ${app}-$build compiled"
				done # FOR EACH APP
			fi
		fi
	done
	
	echo 'compilation finished.'
}

function setGovernor {
	
	governor=$1
	for processor in $(cat /proc/cpuinfo | grep processor | awk '{print $3}');
	do
		sudo cpufreq-set -c $processor -g $governor
		test $governor == 'userspace' && sudo cpufreq-set -c $processor -f ${!governor}
	done
}

function restoreGovernor {

	governor='ondemand'
	for processor in $(cat /proc/cpuinfo | grep processor | awk '{print $3}');
	do
		sudo cpufreq-set -c $processor -g $governor
	done
}

function execute {

	test -e $output || mkdir $output
	
	
	test -z "$NEXEC"         && NEXEC=20
	test -z "$NB_CORES"      && NB_CORES='1 2 4 8'
	test -z "$MEM_ALLOCS"    && MEMALLOCS='ptmalloc'
	test ! -z "$MEM_ALLOCS"  && MEMALLOCS=$MEM_ALLOCS
	test -z "$GOVS"          && GOVERNORS='ondemand'
	test ! -z "$GOVS"        && GOVERNORS=$GOVS
	test ! -z "$STM_DESIGNS" && DESIGNS=$STM_DESIGNS
	test ! -z "$STM_CMS"     && CMS=$STM_CMS
	test ! -z "$STAMP_APP"   && APPS=$STAMP_APP
	test ! -z "$RTM_CMS"     && RTMCMS=$RTM_CMS
	
	echo
	echo 'starting execution...'

	for app in ${APPS}; do
		eval exec_flags=\$EXEC_FLAG_$app
		for build in $BUILDS; do
			SUFIXES=$build
			test $build == 'tinystm' && SUFIXES="$(eval echo -n {${DESIGNS// /,}}-{${CMS// /,}} | sed 's|{||g;s|}||g')"
			test $build == 'tsx-rtm' && SUFIXES="$(eval echo -n $build-{${RTMCMS// /,}} | sed 's|{||g;s|}||g')"
			NTHREADS=$NB_CORES
			test $build == "seq" && NTHREADS='1'
			SUFIXES="$(eval echo -n {${SUFIXES// /,}}-{${GOVERNORS// /,}} | sed 's|{||g;s|}||g')"
			for sufix in $SUFIXES; do
				regex='-([a-z]+)$' # regular expression to get governor from sufix variable
				[[ $sufix =~ $regex ]] && governor=${BASH_REMATCH[1]} && setGovernor $governor
				appRunPath="stamp/$build/$app/$app-$(sed 's/-[a-z]\+$//' <<< $sufix)" # remove governor from sufix
				timeOutput="$output/$app-$sufix.time"
				timeLog="$output/$app-$sufix.timelog"
				abortOutput="$output/$app-$sufix.abort"
				energyOutput="$output/$app-$sufix.energy"
				energyLog="$output/$app-$sufix.energylog"
				echo "#$MEMALLOCS" | sed 's/ /\t/g' > $timeOutput
				echo "#$MEMALLOCS" | sed 's/ /\t/g' > $timeLog
				echo "#$MEMALLOCS" | sed 's/ /\t/g' > $energyOutput
				echo "#$MEMALLOCS" | sed 's/ /\t/g' > $energyLog
				for memalloc in $MEMALLOCS; do
					timeTemp="$memalloc.time"
					timeLogTemp="$memalloc.timelog"
					abortTemp="$memalloc.abort"
					energyTemp="$memalloc.energy"
					energyLogTemp="$memalloc.energylog"
					for i in $NTHREADS; do
						rm -f *.temp
						for((j=0;j<${NEXEC};j++)); do
							echo "execution $j: ./$appRunPath ${exec_flags}$i ($memalloc) ($governor)"	
							if [ $build == "tsx-rtm" -o $build == "tsx-hle" ]; then
								PCM_FLAGS=$(eval eval echo -n $PCM_FLAGS)
								eval sudo ./$PCM_PATH/pcm-tsx.x \"LD_PRELOAD=${!memalloc} ./${appRunPath} ${exec_flags}$i\" $PCM_FLAGS > data.temp
							else
								sudo LD_PRELOAD=${!memalloc} ./${appRunPath} ${exec_flags}$i  > data.temp
							fi
							eval $GET_TIME   >> time.temp
							eval $GET_ABORT  >> abort.temp
							eval $GET_ENERGY >> energy.temp
						done # FOR EACH EXECUTION
						echo "$(awk "$awkscript" time.temp)"   >> $timeTemp
						echo "$(awk "$awkscript" abort.temp)"  >> $abortTemp
						echo "$(awk "$awkscript" energy.temp)" >> $energyTemp
						cat time.temp >> $timeLogTemp
						echo >> $timeLogTemp
						cat energy.temp >> $energyLogTemp
						echo >> $energyLogTemp
					done # FOR EACH NUMBER OF THREADS
				done # FOR EACH MEMORY ALLOCATOR
				eval paste "$(for m in $MEMALLOCS; do echo $m".time";      done | tr '\n' ' ')" >> $timeOutput
				eval paste "$(for m in $MEMALLOCS; do echo $m".timelog";   done | tr '\n' ' ')" >> $timeLog
				eval paste "$(for m in $MEMALLOCS; do echo $m".abort";     done | tr '\n' ' ')" >> $abortOutput
				eval paste "$(for m in $MEMALLOCS; do echo $m".energy";    done | tr '\n' ' ')" >> $energyOutput
				eval paste "$(for m in $MEMALLOCS; do echo $m".energylog"; done | tr '\n' ' ')" >> $energyLog
				rm *.{time,timelog,abort,energy,energylog}
				restoreGovernor # reset system governor to ondemand
			done # FOR EACH SUFIX
		done # FOR EACH BUILD
	done # FOR EACH APPS
	rm -f *.temp

	echo 'execution finished.'
}

function clean {
	
	echo 'starting cleanup...'
	
	test -e tinySTM/Makefile && rm tinySTM/Makefile
	make -C tinySTM/ -f Makefile.template clean
	make -C tsx/rtm -f Makefile.template clean
	make -C msr/ clean
	rm -rf stamp/{seq,tinystm,lock,tsx-rtm,tsx-hle}
	make -C IntelPCM clean

	echo 'cleanup finished.'
	
}

source plotFunctions.sh

function plotstats {
		
	echo 'starting to plot...'

	test -z "$NB_CORES"      && NB_CORES='1 2 4 8'
	test ! -z "$STM_DESIGNS" && DESIGNS=$STM_DESIGNS
	test ! -z "$STM_CMS"     && CMS=$STM_CMS
	test ! -z "$STAMP_APP"   && APPS=$STAMP_APP
	
	case $PLOT in
		STAMP-speedup)
			plot-STAMP-speedup ;;
		STAMP-energy)
			plot-STAMP-energy ;;
		STAMP-rtm)
			plot-STAMP-rtm ;;
		STAMP-edp)
			plot-STAMP-edp ;;
		HLE-RTM-tinySTM)
			plot-HLE-RTM-tinySTM ;;
		*)
			echo "a plot style must be specified!" ;;
	esac

	echo 'plot finished.'
}

function cleanup {
	
	rm -f *.temp
	rm -f *.{time,timelog,abort,energy,energylog}
	test -d output-data && rm -f output-data/*.{png,eps,table}
	restoreGovernor
	exit
}

trap cleanup SIGINT

if [ "$#" -eq "0" ]; then
	echo $0' : error - missing parameters or arguments'
	echo 'for more information run "'$0' help"'
else
	run_opt=$1
	shift
	while getopts ":d:D:m:a:b:t:n:p:g:M:P:" opt;
	do
		case $opt in
			d) STM_DESIGNS=$OPTARG ;;
			m) STM_CMS=$OPTARG ;;
			D) RTM_CMS=$OPTARG ;;
			a) STAMP_APP=$OPTARG ;;
			b) BUILDS=$OPTARG ;;
			t) NB_CORES=$OPTARG ;;
			n) NEXEC=$OPTARG ;;
			p) PLOT=$OPTARG ;;
			g) GOVS=$OPTARG ;;
			M) MEM_ALLOCS=$OPTARG ;;
			P) PCM_PATH=$OPTARG ;;
			\?) echo $0" : error - invalid option -- $OPTARG"
				exit 1
		esac
	done

	case $run_opt in
		help)
			usage
			;;
		comp)
			compile
			;;
		exec)
			execute
			;;
		clean)
			clean
			;;
		plot)
			plotstats
			;;
		*)
			echo $0" : erro - invalid parameter - $run_opt"
			echo 'for more information run "'$0' help"'
			;;
	esac
fi
