#!/usr/bin/env python3

import sys

timesteps = []

try:
	filename = sys.argv[1]
	n = int(sys.argv[2])
except:
	print("Usage: %s filename.log n_magnets [strip_amount]")
	exit(1)

strip_amount = n+1
try:
	strip_amount = int(sys.argv[3])
except:
	pass


t_sum = 0
for line in open(filename).readlines():
	if line.startswith("TIM1_CCR1 = "):
		ccr = int(line[12:])
		timesteps.append((t_sum, ccr))
		t_sum += ccr

# strip first few timesteps
timesteps = timesteps[strip_amount:]

m = (timesteps[n][1] - timesteps[0][1]) / (timesteps[n][0] - timesteps[0][0])

distances = [ timesteps[i][1] - m * (timesteps[i][0] - timesteps[0][0]) for i in range(n) ]

print([n*d/sum(distances) for d in distances])
print([int(1000*(2**16)*n*d/sum(distances)) for d in distances])


