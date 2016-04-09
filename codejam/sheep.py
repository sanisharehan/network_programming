print "Hello"
n = 1692
i = 1
done = False
l = []

def get_numbers_in_n(n):
	m = str(n)
	for ch in m:
		if ch not in l:
			l.append(ch)
	print l

while(not done):
	c = n * i
	get_numbers_in_n(c)
	if len(l) == 10:
		print "Last input was: ", c
		print "Last iteration was: ", i
		done = True
	else:
		i = i+1

