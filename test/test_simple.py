import unittest
from testbase import LightCacheTestBase

class SimpleTest(LightCacheTestBase):
    
    def test_simple(self):
        print "Simple test:"
        self.client.set("key", "value", 5)
        self.assertEqual(self.client.get("key"), "value")
        self.client.set("key", "value2", 5)
        self.assertEqual(self.client.get("key"), "value2")
       
if __name__ == '__main__':
    unittest.main()

