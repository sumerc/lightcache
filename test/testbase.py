import unittest
import socket
import testconf
from lcclient import LightCacheClient
from protocolconf import *

def make_client():
    if testconf.use_unix_socket:
        client = LightCacheClient(socket.AF_UNIX, socket.SOCK_STREAM) 
        client.connect(testconf.unix_socket_path)
    else:    
        client = LightCacheClient(socket.AF_INET, socket.SOCK_STREAM)  	        
        client.connect((testconf.host, testconf.port)) 
    return client

class LightCacheTestBase(unittest.TestCase):
     
    def setUp(self):
        self.client = make_client()
        
    def tearDown(self):
        self.client.close()
        
    def assertDisconnected(self, in_secs=None):
        assert self.client.is_disconnected(in_secs) == True
        
    def assertErrorResponse(self, err):
        assert self.client.response.errcode == err, "Expected %d but got %d." % (err, self.client.response.errcode)
    
    def assertKeyNotExists(self, key):
        self.assertEqual(self.client.get(key), None)
        self.assertErrorResponse(KEY_NOTEXISTS)
    
    def _stats2dict(self, stats):
        result = {}
        stats = stats.split("\r\n")
        for stat in stats:
            stat = stat.split(":")
            if (len(stat)) < 2:
                continue;
            result[stat[0]] = stat[1]	    
        return result

    def check_for_memusage_delta(self, cmd_list, delta=0):
        """
        executes the commands and checks for the mem usage delta
        via GET_STATS command before/after execution.
        """
        pstats = self._stats2dict(self.client.get_stats())
        for cmd_tpl in cmd_list:	    
            cmd = getattr(self.client, cmd_tpl[0])
            cmd_args = cmd_tpl[1:]
            cmd(*cmd_args)	    
        cstats = self._stats2dict(self.client.get_stats())
        self.assertEqual(int(pstats["mem_used"])+delta, int(cstats["mem_used"]))

