#!/usr/bin/env python

import os
import sys

def main():
    
    sys.path.insert(0, "../test")
    import testconf
    
    pre_cmd = ""
    try:
        pre_cmd = " ".join(sys.argv[1:]) # cmd to be prepended
    except:
        pass
        
    params = ""
    if testconf.use_unix_socket:
        params += "-s %s" % (testconf.unix_socket_path)
    
    cmd = pre_cmd + " ../src/lightcache" + " " + params
    print "Executing %s..." % (cmd)    
    os.system(cmd)

if __name__ == "__main__":
    main()

