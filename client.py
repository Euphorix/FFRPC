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
		dest_msg_body = ''
		body  = ''
		#string      dest_namespace;
		body += struct.pack("!i", 0)
		#string      dest_service_name;
		body += struct.pack("!i", len(service_name)) + service_name
		#string      dest_msg_name;
		body += struct.pack("!i", len(req_msg.__name__)) + req_msg.__name__
		#uint64_t    dest_node_id;
		body += struct.pack("!i", 0) + struct.pack("!i", 0)
		#string      from_namespace;
		body += struct.pack("!i", 0)
		#string      from_msg_name;
		body += struct.pack("!i", 0)
		#uint64_t    from_node_id;
		body += struct.pack("!i", 0) + struct.pack("!i", 0)
		#int64_t     callback_id;
		body += struct.pack("!i", 0) + struct.pack("!i", 0)
		#string      body;
		body += struct.pack("!i", len(dest_msg_body)) + dest_msg_body
		#string      err_info;
		body += struct.pack("!i", 0)

		head = struct.pack("!IHH", len(body), cmd, 0)
		data = head + body

		print('data len=', len(data))
		tcpCliSock.send(data)
		#先读取包头，在读取body
		need_head_len = 8
		head_recv     = ''
		body_recv     = ''
		try:
			while len(head_recv) < 8:
				head_recv += tcpCliSock.recv(8 - len(head_recv))
			head_parse = struct.unpack('!IHH', head_recv)
			body_len   = head_parse[0]
			#开始读取body
			while len(body_recv) < body_len:
				body_recv += tcpCliSock.recv(body_len - len(body_recv))
			#解析body
			#string      dest_namespace;
			msg_parse = struct.unpack("!IIIIIIIIIIIII", body_recv)
			err_info_len = msg_parse[12]
			if err_info_len != 0:
				self.err_info = body_recv[13*4:]
			else:
				msg_parse_body = body_recv[13*4:]
				#decode
		except:
			return False
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
