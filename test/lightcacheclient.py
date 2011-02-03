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
	
	# data/extra lengths are 32 bit UINTs. Convert them with ntohl
	data_len = socket.htonl(data_len)
	extra_len = socket.htonl(extra_len)

	request = struct.pack('BBII', cmd, key_len, data_len, extra_len)
	request += "%s%s%s" % (key, data, extra)
	return request

    def send_packet(self, **kwargs):	
	data = self._make_packet(**kwargs)	
	super(LightCacheClient, self).send(data)

    def recv_packet(self):
    	try:
	    resp = self.recv(self.RESP_HEADER_SIZE)
 	    opcode, data_len = struct.unpack("BI", resp)
	    data_len = socket.ntohl(data_len)
	    resp = self.recv(data_len)
	    return resp
	except:
	    return None

    def send_raw(self, data):
	self.send(data)    

    def chg_setting(self, key, value):
	self.send_packet(key=key, data=value, command=self.CMD_CHG_SETTING)	
   
    def get_setting(self, key):	
	self.send_packet(key=key, command=self.CMD_GET_SETTING)
	r = struct.unpack("I", self.recv_packet())
	r = socket.ntohl(r[0])
	return r
            
    def set(self, key, value, timeout):
	self.send_packet(key=key, data=value, command=self.CMD_SET, extra=timeout)	   

    def get(self, key):	
	self.send_packet(key=key, command=self.CMD_GET) 
	return self.recv_packet()
        

    def get_stats(self):
	self.send_packet(command=self.CMD_GET_STATS)
	return self.recv_packet()
