import time
import unittest
import struct
import socket

class LightCacheClient(socket.socket):
    CMD_GET = 0x00
    CMD_SET = 0x01  
    CMD_CHG_SETTING = 0x02
    CMD_GET_SETTING = 0X03
	
    EVENT_TIMEOUT = 1 # in sec, (used for time critical tests, shall be added to every timing test code)
    IDLE_TIMEOUT = 2 + EVENT_TIMEOUT # in sec  
    
    PROTOCOL_MAX_KEY_SIZE = 250
    PROTOCOL_MAX_DATA_SIZE = 1024 + PROTOCOL_MAX_KEY_SIZE
    
    def is_disconnected(self, in_secs=None):
	if in_secs:
	    self.settimeout(in_secs)
	try:
	    return (self.recv(1) == "")    
	except socket.timeout: # no disconnect signal
	    return False
	except socket.error: # peer disconnect signal
	    return True

    def _make_packet(self, **kwargs):
	cmd = kwargs.pop("command", 0)
	key = kwargs.pop("key", "")
	key_len = kwargs.pop("key_length", len(key))
	data = kwargs.pop("data_length", "")
	data_len = kwargs.pop("data_length", len(data))
	
	request = struct.pack('BBI', cmd, key_len, data_len)
	request += "%s %s" % (key, data)
	return request

    def send_packet(self, **kwargs):	
	data = self._make_packet(**kwargs)	
	super(LightCacheClient, self).send(data)
    
    def send_raw(self, data):
	self.send(data)    

    def chg_setting(self, key, value):
	self.send_packet(key=key, data=value, command=self.CMD_CHG_SETTING)	
   
    def get_setting(self, key):
	self.send_packet(key=key, command=self.CMD_GET_SETTING)
	return self.recv(1024)
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
    """
    def test_send_overflow_header(self):
	self.client.send_raw("OVERFLOWHEADER")
	self.assertEqual(self.client.is_disconnected(), True)
    
    def test_send_overflow_key(self):
	data = "DENEME"
	self.client.send_packet(data, key_length=self.client.PROTOCOL_MAX_KEY_SIZE)    

    def test_send_overflow_data(self):
	data = "A" *  (self.client.PROTOCOL_MAX_DATA_SIZE+1)
	self.client.send_packet(data, data_length=1)

    def test_chg_setting(self):
	self.client.chg_setting("idle_client_timeout", "2")
    
    def test_invalid_packets(self):
	self.client.send_packet("data_value", key_length=10, command=self.client.CMD_CHG_SETTING, data_length=12)   
    """
    def test_get_setting(self):
	print self.client.get_setting("idle_client_timeout")
    
if __name__ == '__main__':
    unittest.main()

