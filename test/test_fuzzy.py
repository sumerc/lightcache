import unittest
import random
import string
from testbase import LightCacheTestBase

class FuzyyTests(LightCacheTestBase):

    def _rand_string(self, N):	
        return ''.join(random.choice(string.ascii_uppercase + string.digits) for x in range(N))

    def test_fuzzy_headers(self):
        try:    
            N = random.randint(50, self.client.PROTOCOL_MAX_DATA_SIZE+10)
            self.client.send_raw(self._rand_string(N))
        except Exception,e:
            pass # server disconnects while we may be still sending
        self.client.assertDisconnected()

    def test_invalid_command(self):
        self.client.send_packet(command=20)
        #self.client.assertDisconnected(in_secs=1)

if __name__ == '__main__':
    unittest.main()

