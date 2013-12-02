from socket import *
import struct
import time


HOST = 'localhost'
PORT = 21564
BUFSIZ = 1024

class ffclient_t:
     def __init__(self, host, port):
          self.host = host
          self.port = port
          self.err_info = ''
     def error_msg(self):
          return self.err_info
     def connect(self, host, port):
          tcpCliSock = None
          try:
               tcpCliSock = socket(AF_INET, SOCK_STREAM)
               ADDR = (host, port)
               tcpCliSock.connect(ADDR)
          except Exception as e:
               tcpCliSock.close()
               tcpCliSock = None
               self.err_info = 'can\'t connect to dest address error:'+str(e)
          except:
               tcpCliSock.close()
               tcpCliSock = None
               self.err_info = 'can\'t connect to dest address'
          return tcpCliSock
     def call(self, service_name, req_msg, ret_msg):
          self.err_info = ''
          tcpCliSock = self.connect(self.host, self.port)
          if None == tcpCliSock:
               return False
          cmd  = 10241
          body = 'FF'
          head = struct.pack("!ihh", len(body), cmd, 0)
          data = head + body
          print('data len=', len(data))
          tcpCliSock.send(data)
          time.sleep(1)
          tcpCliSock.close()

def foo():
     
     

     while 1:
          data = raw_input('> ')
          if not data: break 
          tcpCliSock.send(data)
          data = tcpCliSock.recv(1024)
          if not data: break
          print('recv:', data)
     print data

     tcpCliSock.close()


ffc = ffclient_t(HOST, PORT)
soc = ffc.connect(HOST, PORT)
print('connect', soc, ffc.error_msg())
ffc.call('', '', '')
