from socket import socket, AF_INET, SOCK_STREAM
from time import sleep
import sys
try:
	data = "TMB /home/oktaka/domains/oktaka.net/public_html/img/sumer.jpg\n"
	port = 6666
	hostname = 'oktaka.com'
	s = socket(AF_INET,SOCK_STREAM)
	s.connect((hostname, port))
	s.send(data)
	data = s.recv(10000)
	if len(data) == 0:
		raise Exception, ""
	f = open('test_received.JPG', 'w+')
	f.write(data)
	f.close()
	print "[+] Test passed! (%s)" % (sys.argv[0])
except Exception, e:
	print "[-] Test failed! (%s), (%s)" % ( sys.argv[0], str(e) )

