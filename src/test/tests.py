import time
import unittest
import struct
import socket

class LightCacheClient(socket.socket):
    CMD_GET = 0x00
    CMD_SET = 0x01  
    CMD_CHG_SETTING = 0x02
    CMD_GET_SETTING = 0X03
    CMD_GET_STATS = 0X04
	
    EVENT_TIMEOUT = 1 # in sec, (used for time critical tests, shall be added to every timing test code)
    IDLE_TIMEOUT = 2 + EVENT_TIMEOUT # in sec  
    
    PROTOCOL_MAX_KEY_SIZE = 250
    PROTOCOL_MAX_DATA_SIZE = 1024 + PROTOCOL_MAX_KEY_SIZE

    RESP_HEADER_SIZE = 8 # in bytes, sync this(xxx)
    
    def _is_disconnected(self, in_secs=None):
	if in_secs:
	    self.settimeout(in_secs)
	try:
	    return (self.recv(1) == "")    
	except socket.timeout: # no disconnect signal
	    return False
	except socket.error: # peer disconnect signal
	    return True
    
    def assertDisconnected(self):
	assert(self._is_disconnected() == True)

    def _make_packet(self, **kwargs):
	cmd = kwargs.pop("command", 0)
	key = kwargs.pop("key", "")
	key_len = kwargs.pop("key_length", len(key))
	data = kwargs.pop("data", "")
	data_len = kwargs.pop("data_length", len(str(data)))
	extra = kwargs.pop("extra", "")
	extra_len = kwargs.pop("extra_length", len(str(extra)))
	request = struct.pack('BBII', cmd, key_len, data_len, extra_len)
	request += "%s%s%s" % (key, data, extra)
	return request

    def send_packet(self, **kwargs):	
	data = self._make_packet(**kwargs)	
	super(LightCacheClient, self).send(data)

    def recv_packet(self):
	resp = self.recv(self.RESP_HEADER_SIZE)
 	opcode, data_len = struct.unpack("BI", resp)
	resp = self.recv(data_len)
	return resp
	
    def send_raw(self, data):
	self.send(data)    

    def chg_setting(self, key, value):
	self.send_packet(key=key, data=value, command=self.CMD_CHG_SETTING)	
   
    def get_setting(self, key):
	self.send_packet(key=key, command=self.CMD_GET_SETTING)
	r = struct.unpack("I", self.recv_packet()) 
	return r[0]
    
    def set(self, key, value, timeout):
	self.send_packet(key=key, data=value, command=self.CMD_SET, extra=timeout)	   

    def get(self, key):
	self.send_packet(key=key, command=self.CMD_GET) 
	return self.recv_packet()
    def get_stats(self):
	self.send_packet(command=self.CMD_GET_STATS)
	return self.recv_packet()

class LightCacheTestBase(unittest.TestCase):
    
    host = 'localhost'
    port = 13131  
     
    def setUp(self):
	self.client = LightCacheClient(socket.AF_INET, socket.SOCK_STREAM)  	        
	self.client.connect((self.host, self.port)) 
    
    def tearDown(self):
	self.client.close()
    
    def test_idle_timeout(self):
	self.client.chg_setting("idle_conn_timeout", 2)
     	self.assertEqual(self.client._is_disconnected(in_secs=3), True)
    
    def test_send_overflow_header(self):
	self.client.send_raw("OVERFLOWHEADER")
	self.client.assertDisconnected()
    
    def test_send_overflow_key(self):
	data = "DENEME"
	self.client.send_packet(data=data, key_length=self.client.PROTOCOL_MAX_KEY_SIZE)    
	self.client.assertDisconnected()	

    def test_send_overflow_data(self):
	data = "A" *  (self.client.PROTOCOL_MAX_DATA_SIZE+1)
	self.client.send_packet(data=data, data_length=1)
	self.client.assertDisconnected()

    def test_chg_setting(self):
	self.client.chg_setting("idle_client_timeout", "2")
    	
    def test_invalid_packets(self):
	self.client.send_packet(data="data_value", key_length=10, command=self.client.CMD_CHG_SETTING, data_length=12)   
    
    def test_get_setting(self):
	self.client.chg_setting("idle_conn_timeout", 5)
	self.assertEqual(self.client.get_setting("idle_conn_timeout"), 5)
    
    def test_set(self):
	self.client.set("key1", "value1", 11)
    
    def test_get(self):
	self.client.set("key2", "value2", 13)
	self.assertEqual(self.client.get("key2"), "value2")
    
    def test_get_stats(self):
	print self.client.get_stats()
   
if __name__ == '__main__':
    unittest.main()

