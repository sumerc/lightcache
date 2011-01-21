import unittest
import struct
from socket import socket, AF_INET, SOCK_STREAM

class LightCacheClient(socket):
    	
    def _make_packet(self, data):
	request = struct.pack('bbI', 0, 0, len(data))
	request += data
	return request

    def send_cmd(self, data):	
	data = self._make_packet(data)	
	super(LightCacheClient, self).send(data)

class LightCacheTestBase(unittest.TestCase):
    
    host = 'localhost'
    port = 13131  
    client = LightCacheClient(AF_INET, SOCK_STREAM)   

    def setUp(self):
        pass

    def tearDown(self):
        pass

    def test_send(self):
        self.client.connect((self.host, self.port))
    	self.client.send_cmd("selam")

if __name__ == '__main__':
    unittest.main()

