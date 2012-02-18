'''
Created on Feb 4, 2012

@author: Tim Biggs
'''

from structlib import *
from common import *
from struct import error
from itertools import izip_longest
import threading
import hashlib
import dao

dbdao = dao.dao()

def runUpdateDaemon():
    for userID in onlineClients.keys():
        if not userID in onlineFriends:
            onlineFriends[userID] = set([])
        
        friendListUpdate = set([])
        fullFriendList = dbdao.getFriends(userID)
        for (_, friendID) in fullFriendList:
            if friendID in onlineClients:
                friendListUpdate.add(friendID)
        # Build lists (actually sets) of friends who went offline or online
        # Offline: Friend exists in old list but not in new one
        # Online: Friend exists in new list but not in old one
        offlinelist = onlineFriends[userID] - (onlineFriends[userID] & friendListUpdate)
        onlinelist = friendListUpdate - (onlineFriends[userID] & friendListUpdate)
        
        # Creating the actual data to send -- this uses some craziness with mapping functions
        # Basically, the first line does the offline list, and the second does the online one
        payload = ''.join(map(upFormat.pack, list(izip_longest(offlinelist, '', fillvalue=0))))
        payload = payload + ''.join(map(upFormat.pack, list(izip_longest(onlinelist, '', fillvalue=1))))
        
        (_, sock) = onlineClients[userID]
        sock.send(padToSize(pktFormat.pack(0, r_status_update, len(payload)) + payload, BUFSIZE))
    threading.Timer(UPDATE_INTERVAL, runUpdateDaemon).start()

class RequestHandler:
    
    def __init__(self, addr, sock):
        self.addr = addr
        self.sock = sock
        
    def __del__(self):
        if not hasattr(self, 'userID'):
            return
        if self.userID in onlineClients:
            del onlineClients[self.userID]
        if self.userID in onlineFriends:
            del onlineFriends[self.userID]
    
    '''
    @param userID: The user id of the requester
    @return: A packet with a list of friends 
    '''
    def init_friendlist_packet(self):
        data = ''
        for friend in dbdao.getFriends(self.userID):
            if len(data) + flEntryFormat.size > DATASIZE:
                break
            (friendname, friendID) = friend
            data = data + flEntryFormat.pack(str(friendname), friendID)
        return padToSize(pktFormat.pack(self.userID, r_login_good, len(data)) + data, BUFSIZE)
    
    ''' Handles login requests
        @return The packet data to send back to the client
    '''
    def handle_login(self, req):
        try:
            (username, pw, dccpPort), _ = decode(clFormat, req)
        except error:
            return errorPacket(e_invalidloginformat)
        
        # note: this application not meant to be ultra secure, or even secure at all
        username = username.replace(chr(0), '')
        info = dbdao.getUser(username)
        if info == None:
            return errorPacket(e_invalidlogin)
        
        (userID, username, pw_hash, pw_salt) = info
        pw = pw.replace(chr(0), '')
        digest = hashlib.sha512(pw + pw_salt).hexdigest()
        # TODO: stop a client from logging in twice
        # if userID in onlineClients:
        #     return loginDuplicateAck
        if pw_hash == digest:
            self.userID = userID
            onlineClients[userID] = (self.addr, dccpPort)
            return self.init_friendlist_packet()
        
        return errorPacket(e_invalid_pw)
    
    def handle_addr_request(self, requesterID, data):
        def toIntVal(ipaddr):
            a = ipaddr.split('.')
            val = 0
            for elem in a:
                val = (val << 8) | int(elem)
            return val
        
        if not requesterID in onlineClients:
            return errorPacket(e_not_logged_in)
        
        try:
            (friendID, ), _ = decode(carFormat, data)
        except error:
            return errorPacket(e_unknown)
        
        # TODO: Filter for whether the two are friends or not
        if not friendID in onlineClients:
            return errorPacket(e_not_logged_in)
        
        (addr, port), _ = onlineClients[friendID]
        payload = carOutFormat.pack(toIntVal(addr), port)
        return padToSize(pktFormat.pack(0, r_request_addr, len(payload)) + payload, BUFSIZE)
    
    def handle_request(self, req):
        try:
            (userID, pType, _), data = decode(pktFormat, req)
        except error:
            return errorPacket(e_unknown)
        
        if pType == r_login:
            return self.handle_login(data)
        elif pType == r_request_addr:
            return self.handle_addr_request(userID, data)
        else:
            return errorPacket(e_unknown)
