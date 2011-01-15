from os import system
from time import time

COUNT = 1000

t0=time()
for _ in range(COUNT):
	print "%d) " % (_)
	system("python testsuccess.py")
print "Elapsed %d secs." % (time()-t0)