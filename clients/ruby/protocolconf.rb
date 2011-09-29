

CMD_GET = 0x00
CMD_SET = 0x01  
CMD_CHG_SETTING = 0x02
CMD_GET_SETTING = 0x03
CMD_GET_STATS = 0x04
CMD_DELETE = 0x05
CMD_FLUSH_ALL = 0x06

EVENT_TIMEOUT = 1 # in sec, (used for time critical tests, shall be added to every timing test code)
IDLE_TIMEOUT = 2 + EVENT_TIMEOUT # in sec  

PROTOCOL_MAX_KEY_SIZE = 250
PROTOCOL_MAX_DATA_SIZE = 1024 + PROTOCOL_MAX_KEY_SIZE

RESP_HEADER_SIZE = 8 # in bytes, SYNC THIS (xxx)

# error definitions
KEY_NOTEXISTS = 0x00
INVALID_PARAM = 0x01
INVALID_STATE = 0x02
INVALID_PARAM_SIZE = 0x03
SUCCESS = 0x04
INVALID_COMMAND = 0x05
OUT_OF_MEMORY = 0x06

def err2str(e)

    assert(e != None)
    assert(e.is_a Integer)       
    
    if e == KEY_NOTEXISTS
        return 'KeyNotExists'
    elsif e == INVALID_PARAM
        return 'InvalidParam'
    elsif e == INVALID_STATE
        return 'InvalidState'
    elsif e == INVALID_PARAM_SIZE
        return 'InvalidParamSize'
    elsif e == SUCCESS
        return 'Success'
    elsif e == INVALID_COMMAND
        return 'InvalidCommand'
    elsif e == OUT_OF_MEMORY
        return 'OutOfMemory'
    end
    raise Exception.new("Unrecognized error code received.[%d]" % [e])
end   

    
