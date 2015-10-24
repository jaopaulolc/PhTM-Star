#!/usr/bin/awk


BEGIN{
	nthreads = 0
	nTx = 0
	STEP=1
}

{
	if($1 == "Thread" && $2 ~ "[0-9]"){
		nthreads++
		nTx=0
		#print $1, nthreads
	} else if($1 ~ "Tx" && $2 ~ "[0-9]"){
		nTx++
		counter[nTx] = 0
		#print $1, nTx
	} else if($1 ~ "^[0-9]*"){
		c = int(counter[nTx] / STEP)
		rSetSize[nTx,c] += $1
		wSetSize[nTx,c] += $2
		counter[nTx]++
	}

}

END{

	for(i=1; i <= nTx; i++){
		file = sprintf("%s-tx%d.setsize",app,i-1)
		for(j=1; j <= int(counter[i]/STEP) + int(counter[i] % STEP); j++){
			print rSetSize[i,j]/(nthreads*STEP), " ", wSetSize[i,j]/(nthreads*STEP) > file
		}
	}
}
