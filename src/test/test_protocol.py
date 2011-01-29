import unittest
from testbase import LightCacheTestBase

class ProtocolTests(LightCacheTestBase):
    
    def test_idle_timeout(self):
	self.client.chg_setting("idle_conn_timeout", 2)
     	self.assertTrue(self.client._is_disconnected(in_secs=4))
    
    def test_send_overflow_header(self):
	self.client.send_raw("OVERFLOWHEADER")
	self.client.assertDisconnected()
    
    def test_send_overflow_key(self):
	data = "DENEME"
	self.client.send_packet(data=data, key_length=self.client.PROTOCOL_MAX_KEY_SIZE)    
	self.client.assertDisconnected()	

    def test_send_overflow_data(self):
	data = "A" *  (self.client.PROTOCOL_MAX_DATA_SIZE+1)
	self.client.send_packet(data=data, data_length=1)
	self.client.assertDisconnected()

    def test_chg_setting(self):
	self.client.chg_setting("idle_client_timeout", "2")
    	
    def test_invalid_packets(self):
	self.client.send_packet(data="data_value", key_length=10, 
		command=self.client.CMD_CHG_SETTING, data_length=12)   
    
    def test_get_setting(self):
	self.client.chg_setting("idle_conn_timeout", 5)
	self.assertEqual(self.client.get_setting("idle_conn_timeout"), 5)
    
    def test_set(self):
	self.client.set("key1", "value1", 11)
    
    def test_get(self):
	self.client.set("key2", "value2", 13)
	self.assertEqual(self.client.get("key2"), "value2")
    
    def test_get_stats(self):
	stats = self._stats2dict(self.client.get_stats())
	self.assertTrue(stats.has_key("mem_used"))

if __name__ == '__main__':
    unittest.main()

