from socket import socket, AF_INET, SOCK_STREAM
from time import sleep
import sys
try:
	data = "TMB sumercip"
	port = 6666
	hostname = 'oktaka.com'
	s = socket(AF_INET,SOCK_STREAM)
	s.connect((hostname, port))
	s.send(data)
	data = s.recv(10000)
	if len(data) == 0:
		print "[+] Test passed! (%s)" % (sys.argv[0])
	else:
		raise Exception, ""
except Exception, e:
	print "[-] Test failed! (%s), (%s)" % ( sys.argv[0], str(e) )
	
