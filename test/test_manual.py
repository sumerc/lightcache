
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
        key = s[1]
        
        value = None
        extra = None
        try:
            value = s[2]
            extra = s[3]
        except IndexError:
            pass             
        
        print "Sending command: %s" % (s)
        
        try:
            resp = None
            if cmd == 'GET':
                resp = client.get(key)
            elif cmd == 'SET':
                resp = client.set(key, value, extra)
            elif cmd == 'CHG_SETTING':
                resp = client.chg_setting(key, value)
            elif cmd == 'GET_SETTING':
                resp = client.chg_setting(key)
            elif cmd == 'GET_STATS':
                resp = client.get_stats(key)    
            print "Received response: %s" % (resp)
        except Exception,e:
            print "Command Error: %s" % (e) 
            continue
