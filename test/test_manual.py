
from lightcacheclient import make_client

if __name__ == '__main__':
    client = make_client()
    
    while(1):
        s = raw_input(">")
        s = s.split(' ')
        cmd = s[0].upper()
        key = s[1]
        
        try:
            value = s[2]
            extra = s[3]
        except IndexError:
            value = None
            extra = None            
        
        if cmd == 'GET':
            client.get(key)
        elif cmd == 'SET':
            client.get(key, value, extra)
        elif cmd == 'CHG_SETTING':
            pass
        elif cmd == 'GET_SETTING':
            pass
        elif cmd == 'GET_STATS':
            pass    
           
