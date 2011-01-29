import unittest
from testbase import LightCacheTestBase

class MemTests(LightCacheTestBase):
    
    def test_memleak_after_get_set(self):
	self.client.set("key1", "value1", 11)
	self.check_for_memusage_delta( [ ("set", "key1", "value2", 11), ] )
	self.check_for_memusage_delta( [ ("set", "key1", "value3", 11), ] )
	self.check_for_memusage_delta( [ ("set", "key1", "value4", 11), ] )

if __name__ == '__main__':
    unittest.main()

