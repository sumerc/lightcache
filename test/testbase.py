import unittest
import socket
from lightcacheclient import make_client

class LightCacheTestBase(unittest.TestCase):
     
    def setUp(self):
        self.client = make_client()
        
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

