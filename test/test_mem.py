import unittest
from testbase import LightCacheTestBase

class MemTests(LightCacheTestBase):

    # TODO:
    # - memleak after key expired and new key is set.
    
    def test_memleak_after_get_set(self):
        self.client.set("key1", "value1", 11)
        self.check_for_memusage_delta( [ 
            ("set", "key1", "value2", 11), 
            ("set", "key1", "value3", 11), 
            ("set", "key1", "value4", 11),
            ])
        
    def test_memleak_after_chg_get_setting(self):
        self.check_for_memusage_delta( [ ("get_setting", "idle_conn_timeout"), ] )
        self.check_for_memusage_delta( [ ("chg_setting", "idle_conn_timeout", 5), ] )

    def test_memleak_after_getstats(self):
        self.check_for_memusage_delta( [ ("get_stats", ), ] )
        
    def test_memleak_after_get_invalid_key(self):
        self.check_for_memusage_delta( [ ("get", "invalid_key" ), ] )
    
    def test_memleak_after_key_expired(self):
        self.client.set("key30", "value30", 1)
        import time;time.sleep(2)
        self.check_for_memusage_delta( [ ("set", "key31", "value31", 11), ] )
    
    
    def test_memleak_test_itself_is_valid(self):
        # below command should create a hash entry on server side.
        self.assertRaises( AssertionError, self.check_for_memusage_delta, ([("set", "key4", "value1", 1),])  )
        
if __name__ == '__main__':
    print "Running MemTests..."
    unittest.main()

