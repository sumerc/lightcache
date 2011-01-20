import unittest
from socket import socket, AF_INET, SOCK_STREAM

class LightCacheClient(socket):
    pass

class LightCacheTestBase(unittest.TestCase):
    
    host = 'localhost'
    port = 13131     

    def setUp(self):
        self.client = LightCacheClient(AF_INET, SOCK_STREAM)
    def tearDown(self):
        pass

    def test_connection(self):
	self.client.connect((self.host, self.port))

if __name__ == '__main__':
    unittest.main()

