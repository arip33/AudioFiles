'''
Created on Feb 4, 2012

@author: Tim Biggs
'''

import asyncore
import socket
import threading
from requesthandler import RequestHandler, runUpdateDaemon
from common import BUFSIZE, UPDATE_INTERVAL

class ChatHandler(asyncore.dispatcher):
    '''
    Handler for the ChatServer module -- 1 loop instance per client
    '''
    
    def __init__(self, sock, addr):
        asyncore.dispatcher.__init__(self, sock=sock)
        self.reqHandler = RequestHandler(addr, sock)
        
    def handle_read(self):
        if not hasattr(self, 'reqHandler'):
            return
        try:
            self.send(self.reqHandler.handle_request(self.recv(BUFSIZE)))
        except socket.error:
            pass
        
    def handle_close(self):
        self.close()
        
    def close(self):
        if hasattr(self, 'reqHandler'):
            del self.reqHandler
        asyncore.dispatcher.close(self)

class ChatServer(asyncore.dispatcher):
    '''
    Main server module -- should only be 1 instance total
    '''

    def __init__(self, host, port):
        '''
        Constructor
        '''
        asyncore.dispatcher.__init__(self)
        
        self.timerCancelled = False
        self.restartTimer()
        
        self.create_socket(socket.AF_INET, socket.SOCK_STREAM)
        self.set_reuse_addr()
        self.bind((host, port))
        self.listen(5)
        
    def restartTimer(self):
        if not self.timerCancelled:
            self.timerThread = threading.Timer(UPDATE_INTERVAL, runUpdateDaemon, [self.restartTimer])
            self.timerThread.start()
    
    def handle_accept(self):
        pair = self.accept()
        if pair is None:
            pass
        else:
            sock,addr = pair
            print 'Incoming connection from %s' % repr(addr)
            ChatHandler(sock, addr) # adds itself to the server loop automatically
            
    def handle_close(self):
        self.close()
        
    def close(self):
        self.timerCancelled = True
        self.timerThread.cancel()
        asyncore.dispatcher.close(self)
