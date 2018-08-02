#!/usr/bin/awk


BEGIN{
	RS="---\n"
	FS="\n"
	minSamples=1.0e12
}

{
	for (i=1; i <= NF; i++) {
		samples[i] += $i 
	}
	if (NF < minSamples)
		minSamples = NF
}

END{
	print header > outfile
	for (i=1; i < minSamples; i++) {
		th[i] = samples[i] / NR
		print th[i] > outfile
	}
}
