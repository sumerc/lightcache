import struct
import socket
import testconf

def make_client():
    if testconf.use_unix_socket:
        client = LightCacheClient(socket.AF_UNIX, socket.SOCK_STREAM) 
        client.connect(testconf.unix_socket_path)
    else:    
        client = LightCacheClient(socket.AF_INET, socket.SOCK_STREAM)  	        
        client.connect((testconf.host, testconf.port)) 
    return client

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

    RESP_HEADER_SIZE = 8 # in bytes, SYNC THIS (xxx)
    
    # error definitions
    KEY_NOTEXISTS = 0x00
    INVALID_PARAM = 0x01
    INVALID_STATE = 0x02
    INVALID_PARAM_SIZE = 0x03
    SUCCESS = 0x04
    
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
        assert self._is_disconnected() == True
        
    def assertErrorResponse(self, err):
        resp = self.recv(self.RESP_HEADER_SIZE)
        try:
            opcode, errcode, data_len = struct.unpack("BBI", resp)
        except:
            print "dpfgjdfgdfg"
            print len(resp)
        assert errcode == err

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

    def send_packet(self, **kwargs):    
        data = self._make_packet(**kwargs)    
        super(LightCacheClient, self).send(data)

    def recv_packet(self):
        try:
            # TODO: recv_header auxiliary function 
            resp = self.recv(self.RESP_HEADER_SIZE)
            opcode, errcode, data_len = struct.unpack("BBI", resp)
            print errcode
            
            if errcode != self.SUCCESS: # extra validation
                return None
            data_len = socket.ntohl(data_len)
            resp = self.recv(data_len)
            return resp
        except:
            return None

    def send_raw(self, data):
        self.send(data)   
    
    def chg_setting(self, key, value):
        assert key is not None
        assert value is not None
        
        self.send_packet(key=key, data=value, command=self.CMD_CHG_SETTING)    
   
    def get_setting(self, key):  
        assert key is not None
      
        self.send_packet(key=key, command=self.CMD_GET_SETTING)    
        resp = self.recv_packet()
        if resp is None:
            return resp
        r = struct.unpack("!Q", resp) # (!) means data comes from network(big-endian)
        return r[0]
            
    def set(self, key, value, timeout):
        assert key is not None
        assert value is not None
        assert timeout is not None
        
        self.send_packet(key=key, data=value, command=self.CMD_SET, extra=timeout)       

    def get(self, key):
        assert key is not None
        
        self.send_packet(key=key, command=self.CMD_GET) 
        return self.recv_packet()
        
    def get_stats(self):
        self.send_packet(command=self.CMD_GET_STATS)
        return self.recv_packet()

