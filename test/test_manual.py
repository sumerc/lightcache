
from protocolconf import *
from lightcacheclient import make_client

if __name__ == '__main__':
    client = make_client()
    
    # before moving to the main loop make sure that the
    # idle timeout is infinite. Autotests may change this.
    client.chg_setting("idle_conn_timeout", 4000000000)
    
    while(1):
        s = raw_input(">")
        s = s.split(' ')
        cmd = s[0].upper()
            
        key = None # get_stats do not require anything
        data = None
        extra = None
        try:            
            key = s[1]
            data = s[2]
            extra = s[3]
        except IndexError:
            pass             
        
        print "Sending command: %s" % (s)
        
        resp = None
        if cmd == 'GET':
            resp = client.get(key)
        elif cmd == 'SET':
            resp = client.set(key, data, extra)
        elif cmd == 'GETQ':
            resp = client.getq(key)
        elif cmd == 'SETQ':
            resp = client.setq(key, data, extra)
        elif cmd == 'CHG_SETTING':
            resp = client.chg_setting(key, data)
        elif cmd == 'GET_SETTING':
            resp = client.get_setting(key)
        elif cmd == 'GET_STATS':
            resp = client.get_stats()
        elif cmd == 'DELETE':
            resp = client.delete(key)
        elif cmd == 'FLUSH_ALL':
            resp = client.flush_all()
        else:
            args = {}
            args["key"] = key
            args["command"] = int(cmd)
            if data:
                args["data"] = data
            if extra:
                args["extra"] = extra                
            client.send_packet(**args)  
            resp = client.recv_packet()
        if resp is not None or client.response.errcode is not None:
            print "Received response data: %s, errcode:%s" % (resp, err2str(client.response.errcode))
        
