import time
import unittest
import struct
from socket import socket, AF_INET, SOCK_STREAM

class LightCacheClient(socket):
    CMD_GET = 0x00
    CMD_SET = 0x01  

    IDLE_TIMEOUT = 2 # in sec  
	
    def is_disconnected(self):
	return (self.recv(1) == "")    

    def _make_packet(self, data, **kwargs):
	klen = kwargs.pop("key_length", 0)
	cmd = kwargs.pop("command", 0)
	
	request = struct.pack('bbI', cmd, klen, len(data))
	request += data
	return request

    def send_raw_cmd(self, data, **kwargs):	
	data = self._make_packet(data, **kwargs)	
	super(LightCacheClient, self).send(data)

    def get(self, key):	
	self.send_raw_cmd(key, key_length=len(key), command=self.CMD_GET)	
	
class LightCacheTestBase(unittest.TestCase):
    
    host = 'localhost'
    port = 13131  
    client = LightCacheClient(AF_INET, SOCK_STREAM)   

    def setUp(self):
        pass

    def tearDown(self):
        pass

    def test_idle_timeout(self):
        self.client.connect((self.host, self.port))
    	time.sleep(self.IDLE_TIMEOUT)
	self.assertEqual(self.is_disconnected(), True)
	    

if __name__ == '__main__':
    unittest.main()

