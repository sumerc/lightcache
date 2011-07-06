import unittest
import socket
from lightcacheclient import LightCacheClient

class LightCacheTestBase(unittest.TestCase):
    
    host = 'localhost'
    port = 13131 
    
    use_unix_socket = True
    #use_unix_socket = False
    unix_socket_path = '/home/sumerc/Desktop/deneme'    
     
    def setUp(self):
        if self.use_unix_socket:
            self.client = LightCacheClient(socket.AF_UNIX, socket.SOCK_STREAM) 
            self.client.connect(self.unix_socket_path)
        else:    
            self.client = LightCacheClient(socket.AF_INET, socket.SOCK_STREAM)  	        
            self.client.connect((self.host, self.port)) 
        
    def tearDown(self):
        self.client.close()
    
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

