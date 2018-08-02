#!/usr/bin/awk


BEGIN{
	RS="---\n"
	FS="\n"
	isFirst = 1
  measureCount = 1
}

{
	phaseCount = 0
	for (i=1; i <= NF; i++) {
		if ($i ~ "#") {
			match($i, "#[0-9]+[ ]+([0-9]+)", m)
			sampleCount=1
			phaseCount++
			nSamples[phaseCount] = m[1]
		} else {
			phaseSamples[phaseCount, measureCount, sampleCount] = $i 
			sampleCount++
		}
	}
  measureCount++
}

END{
	f = sprintf("%s", outfile)
	print header, header > f
	for(phase=1; phase <= phaseCount; phase++){
		phase_sum = 0.0
		for(i=1; i <= nSamples[phase]; i++){
      sample_sum = 0.0
		  for(j=1; j <= NR; j++) {
			  sample_sum += phaseSamples[phase, j, i]
      }
      sample_mean = sample_sum / NR
			phase_sum += sample_mean
      std_sum = 0.0
		  for(j=1; j <= NR; j++) {
        a = phaseSamples[phase, j, i] - sample_mean
			  std_sum += a * a
      }
      std = sqrt(std_sum / NR)
			print sample_mean, std > f
		}
    phase_mean = phase_sum / nSamples[phase]
		printf "%.3f ", phase_mean
	}
	printf "\n"
}
