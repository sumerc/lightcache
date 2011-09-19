import struct
import socket
import testconf
from protocolconf import *

def make_client():
    if testconf.use_unix_socket:
        client = LightCacheClient(socket.AF_UNIX, socket.SOCK_STREAM) 
        client.connect(testconf.unix_socket_path)
    else:    
        client = LightCacheClient(socket.AF_INET, socket.SOCK_STREAM)  	        
        client.connect((testconf.host, testconf.port)) 
    return client
    
class Response:
    opcode = None
    errcode = None
    datalen = None
    data = None
    
    def __str__(self):
        return "%s" % self.data

class LightCacheClient(socket.socket):
    
    response = Response()
    
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
        data = kwargs.pop("data", "")
        data_len = kwargs.pop("data_length", len(str(data)))
        extra = kwargs.pop("extra", "")
        extra_len = kwargs.pop("extra_length", len(str(extra)))
        
        # data/extra lengths are 32 bit UINTs.
        data_len = socket.htonl(data_len)
        extra_len = socket.htonl(extra_len)

        request = struct.pack('BBII', cmd, key_len, data_len, extra_len)
        request += "%s%s%s" % (key, data, extra)
        return request
        
    def _recv_until(self, n):
        # todo: optimize
        i = 0
        resp = ""
        while(i < n):
            resp += self.recv(1)
            i += 1
        return resp
    
    def _recv_header(self):
        resp = self._recv_until(RESP_HEADER_SIZE)
        return struct.unpack("BBI", resp)
        
    def _reset_prev_resp(self):
        self.response.opcode = None
        self.response.errcode = None
        self.response.data_len = None
        self.response.data = None
        
    def send_raw(self, data):
        self._reset_prev_resp()
        super(LightCacheClient, self).send(data)
    
    def send_packet(self, **kwargs):            
        self._reset_prev_resp()        
        data = self._make_packet(**kwargs)    
        super(LightCacheClient, self).send(data)

    def recv_packet(self):
        self.response.opcode, self.response.errcode, self.response.data_len = self._recv_header()          
        self.response.data_len = socket.ntohl(self.response.data_len)
        if self.response.data_len == 0:
            return None 
        self.response.data = self._recv_until(self.response.data_len)
        return self.response.data        
    
    def chg_setting(self, key, value):
        assert key is not None
        assert value is not None
        
        self.send_packet(key=key, data=value, command=CMD_CHG_SETTING)   
        self.recv_packet() 
   
    def get_setting(self, key):  
        assert key is not None
      
        self.send_packet(key=key, command=CMD_GET_SETTING)    
        resp = self.recv_packet()
        if resp:
            r = struct.unpack("!Q", resp) # (!) means data comes from network(big-endian)
            return r[0]
            
    def set(self, key, value, timeout=3600):
        assert key is not None
        assert value is not None
        assert timeout is not None
        
        self.send_packet(key=key, data=value, command=CMD_SET, extra=timeout)
        self.recv_packet()       

    def delete(self, key):
        assert key is not None
        
        self.send_packet(key=key, command=CMD_DELETE)
        self.recv_packet()
        
    def get(self, key):
        assert key is not None
        
        self.send_packet(key=key, command=CMD_GET)
        return self.recv_packet()
            
    def get_stats(self):
        self.send_packet(command=CMD_GET_STATS)
        return self.recv_packet()
        
    def flush_all(self):
        self.send_packet(command=CMD_FLUSH_ALL)
        self.recv_packet()
        
        
    
