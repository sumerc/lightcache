import time
import unittest
from testbase import LightCacheTestBase
from protocolconf import *

class ProtocolTests(LightCacheTestBase):
    
    def test_send_overflow_data(self):
        data = "A" *  (PROTOCOL_MAX_DATA_SIZE+1)
        self.client.send_packet(data=data)
        self.client.assertErrorResponse(INVALID_PARAM_SIZE)
    
    # TODO: Sometimes failing locally, too. Even if we are disconnecting
    # the socket with idle timeout. assertDisconnect() does not work as intended
    # sometimes? In remote, send_overflow_data is failing with same symptoms.
    def test_idle_timeout(self):
        self.client.chg_setting("idle_conn_timeout", 2)
        self.assertTrue(self.client._is_disconnected(in_secs=10))
    
    def test_send_overflow_header(self):
        self.client.send_raw("OVERFLOWHEADER")
        self.client.assertErrorResponse(INVALID_PARAM_SIZE)
    
    def test_send_overflow_key(self):
        data = "DENEME"
        self.client.send_packet(data=data, key_length=PROTOCOL_MAX_KEY_SIZE)    
        self.client.assertErrorResponse(INVALID_PARAM_SIZE)  
        
    def test_invalid_packets(self):
        self.client.send_packet(data="data_value", key_length=10, 
        command=CMD_CHG_SETTING, data_length=12)   
   
    def test_set(self):
        self.client.set("key1", "value1", 11)
    
    def test_get_stats(self):
        stats = self._stats2dict(self.client.get_stats())
        self.assertTrue(stats.has_key("mem_used"))
    
    def test_get(self):
        self.client.set("key2", "value2", 13)
        self.assertEqual(self.client.get("key2"), "value2")
        
    def test_get_update_same_key(self):
        self.client.set("key", "value", 5)
        self.assertEqual(self.client.get("key"), "value")
        self.client.set("key", "value2", 5)
        self.assertEqual(self.client.get("key"), "value2")    
        
    def test_get_overflowed_timeout(self):
        self.client.set("key5", "value5", 101010101010010101001010101001)
        self.client.assertErrorResponse(INVALID_PARAM)
                
    def test_get_invalid_timeout(self):
        self.client.set("key5", "value5", "invalid_value")
        self.client.assertErrorResponse(INVALID_PARAM)

    def test_get_setting(self):
        self.client.chg_setting("idle_conn_timeout", 5)
        self.assertEqual(self.client.get_setting("idle_conn_timeout"), 5)
        self.client.chg_setting("mem_avail", 65)
        self.assertEqual(self.client.get_setting("mem_avail"), 65)
        
    def test_get_setting_invalid(self):
        self.client.get_setting("invalid_setting")
        self.client.assertErrorResponse(INVALID_PARAM, False) # response is already gotten

    def test_chg_setting(self):
        self.client.chg_setting("idle_conn_timeout", 2)
        self.assertEqual(self.client.get_setting("idle_conn_timeout"), 2)
           
    def test_chg_setting_64bit_value(self):
        self.client.chg_setting("idle_conn_timeout", 0x1234567890)
        self.assertEqual(self.client.get_setting("idle_conn_timeout"), 0x1234567890)
        self.client.chg_setting("idle_conn_timeout", 2) # rollback
        self.assertEqual(self.client.get_setting("idle_conn_timeout"), 2)
    
    def test_chg_setting_invalid(self):
        self.client.chg_setting("idle_conn_timeout", "invalid_value")
        self.client.assertErrorResponse(INVALID_PARAM)

    def test_chg_setting_overflowed(self):
        self.client.chg_setting("idle_conn_timeout", 2222222222222222222222222222222)
        self.client.assertErrorResponse(INVALID_PARAM)
        
    def test_chg_setting_invalid(self):
        self.client.chg_setting("invalid_setting_key", 1)
        self.client.assertErrorResponse(INVALID_PARAM)
    
    def test_subsequent_packets(self):
        self.client.set("key_sub", "val_sub", 60)
        self.client.send_packet(key="key_sub", command=CMD_GET) 
        self.client.send_packet(key="key_sub", command=CMD_GET)
        self.assertEqual(self.client.recv_packet(), "val_sub")
        self.assertEqual(self.client.recv_packet(), "val_sub")

    def test_get_with_timeout(self):
        self.client.set("key2", "value3", 2)
        time.sleep(1)
        self.assertEqual(self.client.get("key2"), "value3")
        time.sleep(2)
        self.assertEqual(self.client.get("key2"), None)  # key expired
        self.client.assertErrorResponse(KEY_NOTEXISTS, False)
    
    def test_send_overflow_header(self):
        self.client.send_raw("OVERFLOWHEADER")
        self.client.assertErrorResponse(INVALID_PARAM_SIZE)
        
            
if __name__ == '__main__':
    print "Running ProtocolTests..."
    unittest.main()

