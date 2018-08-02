#!/usr/bin/awk

BEGIN{
	count = 1
}

{
	y12[count] = $1
	y34[count] = $2
	count++
}

END{
	c=asort(y12)
	print y12[1] - (y12[1] / 10.0)
	print y12[c] + (y12[1] / 10.0)
	c=asort(y34)
	print y34[1] - (y34[1] / 10.0)
	print y34[c] + (y34[1] / 10.0)
}
