import unittest
from testbase import LightCacheTestBase
from protocolconf import *

class MemTests(LightCacheTestBase):
    
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
    
    # TODO: unfortunately we need to find another way of testing this, as all items
    # are recycled, the second time autotests is run this fails normally.   
    #def test_memleak_test_itself_is_valid(self):
        # below command should create a hash entry on server side.
    #    self.assertRaises( AssertionError, self.check_for_memusage_delta, ([("set", "a_unique_long_key_to_be_malloced", "value1", 1),])  )
    
    def test_out_of_memory(self):
        i = 0        
        while(True):
            self.client.set("key%d" % (i), "value(%d)" % (i), 100000)
            if self.client.response.errcode == OUT_OF_MEMORY:
                break
            i += 1 
        # we are out of memory here, try to set some value
        self.client.set("oom_key", "oom_val", 1)
        # TODO:        

if __name__ == '__main__':
    print "Running MemTests..."
    unittest.main()

