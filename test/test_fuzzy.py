import unittest
import random
import string
from testbase import LightCacheTestBase
from protocolconf import *

class FuzyyTests(LightCacheTestBase):

    def _rand_string(self, N):	
        return ''.join(random.choice(string.ascii_uppercase + string.digits) for x in range(N))

    def test_fuzzy_headers(self):
        try:    
            N = random.randint(50, PROTOCOL_MAX_DATA_SIZE+10)
            self.client.send_raw(self._rand_string(N))
        except Exception,e:
            pass # server disconnects while we may be still sending
        self.client.recv_packet()
        self.assertErrorResponse(INVALID_PARAM_SIZE)

    def test_invalid_command(self):
        self.client.send_packet(command=20)
        self.client.recv_packet()
        self.assertErrorResponse(INVALID_COMMAND)

if __name__ == '__main__':
    print "Running FuzzyTests..."
    unittest.main()

