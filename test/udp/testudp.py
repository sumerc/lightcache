from socket import socket, AF_INET, SOCK_DGRAM
data = 'A' * 28
port = 6666
hostname = '127.0.0.1'
udp = socket(AF_INET,SOCK_DGRAM)
udp.sendto(data, (hostname, port))

