import time
import unittest
import struct
import socket

class LightCacheClient(socket.socket):
    CMD_GET = 0x00
    CMD_SET = 0x01  
	
    EVENT_TIMEOUT = 1 # in sec, (used for time critical tests, shall be added to every timing test code)
    IDLE_TIMEOUT = 2 + EVENT_TIMEOUT # in sec  
    PROTOCOL_MAX_DATA_SIZE = 2048

    def is_disconnected(self, in_secs=None):
	if in_secs:
	    self.settimeout(in_secs)
	try:
	    return (self.recv(1) == "")    
	except socket.timeout: # no disconnect signal
	    return False
	except socket.error: # peer disconnect signal
	    return True

    def _make_packet(self, data, **kwargs):
	key_len = kwargs.pop("key_length", 0)
	cmd = kwargs.pop("command", 0)
	data_len = kwargs.pop("data_length", len(data))
	
	request = struct.pack('bbI', cmd, key_len, data_len)
	request += data
	return request

    def send_packet(self, data, **kwargs):	
	data = self._make_packet(data, **kwargs)	
	super(LightCacheClient, self).send(data)
    
    def send_raw(self, data):
	self.send(data)    

    def get(self, key):	
	self.send_packet(key, key_length=len(key), command=self.CMD_GET)	
	
class LightCacheTestBase(unittest.TestCase):
    
    host = 'localhost'
    port = 13131  
     
    def setUp(self):
	self.client = LightCacheClient(socket.AF_INET, socket.SOCK_STREAM)  	        
	self.client.connect((self.host, self.port)) 
    
    def tearDown(self):
	self.client.close()

    #def test_idle_timeout(self):
    #	self.assertEqual(self.client.is_disconnected(in_secs=self.client.IDLE_TIMEOUT), True)
    
    def test_send_overflow_header(self):
	self.client.send_raw("OVERFLOWHEADER")
	self.assertEqual(self.client.is_disconnected(), True)
    
    def test_send_overflow_data(self):
	data = "A" *  (self.client.PROTOCOL_MAX_DATA_SIZE+1)
	self.client.send_packet(data, data_length=1)
    
    

if __name__ == '__main__':
    unittest.main()

