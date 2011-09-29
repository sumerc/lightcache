#require 'cstruct'
require 'socket'
require 'protocolconf'
require 'timeout'

class Response

    @opcode = nil
    @errcode = nil
    @datalen = nil
    @data = nil

    def to_s
        return "%s" % [@data]
    end

end

class LightCacheClient < TCPSocket
    
    

    private
    #def make_packet(**kwargs)
    #end
    
    public
    @response = Response.new()

    def is_disconnected(in_secs=nil)
        
        begin
            timeout(in_secs) do
                return (self.recv(1) == "")
            end
        rescue Timeout::Error
            return false
        rescue Exception
            return true
        end
    end   
    
    
    
end

cli = LightCacheClient.new('localhost', 13131)
puts cli.is_disconnected(1)



