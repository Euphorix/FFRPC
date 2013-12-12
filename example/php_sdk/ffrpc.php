<?php
$GLOBALS['THRIFT_ROOT'] = './';
/* Dependencies. In the proper order. */
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/Transport/TTransport.php';
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/Transport/TSocket.php';
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/Protocol/TProtocol.php';
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/Protocol/TBinaryProtocol.php';
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/Transport/TBufferedTransport.php';
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/Type/TMessageType.php';
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/Factory/TStringFuncFactory.php';
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/StringFunc/TStringFunc.php';
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/StringFunc/Core.php';
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/Type/TType.php';
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/Exception/TException.php';
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/Exception/TTransportException.php';
require_once $GLOBALS['THRIFT_ROOT'].'/Thrift/Exception/TProtocolException.php';

class ffrpc_t {
	public $host = '';
	public $port = 0;
	public $timeout=0;
	public $err_info = '';
	static public function encode_msg($req)
	{
		return "";
	}
	public function __construct($host, $port, $timeout = 0) {
		$this->host = $host;
		$this->port = $port;
		$this->timeout = $timeout;
	}
	public function call($service_name, $req, $ret, $namespace_ = '') {
		//error_reporting(E_ALL);
		echo "tcp/ip connection \n";
		$service_port = 21563;
		$address = '127.0.0.1';

		$socket = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
		if ($socket === false) {
			echo "socket_create() failed: reason: " . socket_strerror(socket_last_error()) . "\n";
		} else {
			echo "OK. \n";
		}

		echo "Attempting to connect to '$address' on port '$service_port'...\n";
		$result = socket_connect($socket, $address, $service_port);
		if($result === false) {
			$this->err_info =  "socket_connect() failed." . socket_strerror(socket_last_error($socket));
			socket_close($socket);
			return false;
		}
		
		$cmd  = 5;
		$dest_msg_body = ffrpc_t::encode_msg($req);
		$body  = '';
		#string      dest_namespace;
		$body .= pack("N", 0);
		#string      dest_service_name;
		$body .= pack("N", strlen($service_name)) . $service_name;
		#string      dest_msg_name;
		$dest_msg_name = $namespace_ . $req->getName();
		#debub print('dest_msg_name', dest_msg_name)
		$body .= pack("N", strlen($dest_msg_name)) . $dest_msg_name;
		#uint64_t    dest_node_id;
		$body .= pack("N", 0) . pack("N", 0);
		#string      from_namespace;
		$body .= pack("N", 0);
		#uint64_t    from_node_id;
		$body .= pack("N", 0) . pack("N", 0);
		#int64_t     callback_id;
		$body .= pack("N", 0) . pack("N", 0);
		#string      body;
		$body .= pack("N", strlen($dest_msg_body)) . $dest_msg_body;
		#string      err_info;
		$body .= pack("N", 0);

		$head = pack("Nnn", strlen($body), $cmd, 0);
		$data = $head . $body;
		
		$out = "";
		echo "sending http head request ...len=".strlen($data)."\n";
		if (false == socket_write($socket, $data))
		{
			$this->err_info =  "socket_write() failed." . socket_strerror(socket_last_error($socket));
			socket_close($socket);
			return false;
		}

		echo "Reading response:\n";
		#先读取包头，在读取body
		$head_recv     = '';
		$body_recv     = '';
		while (strlen($head_recv) < 8)
		{
			$tmp_data = socket_read($socket, 8 - strlen($head_recv));
			if (false == $tmp_data)
			{
				$this->err_info =  "socket_read() failed." . socket_strerror(socket_last_error($socket));
				socket_close($socket);
				return false;
			}
			$head_recv .= $tmp_data;
		}
		$head_parse = unpack('N', $head_recv);
		var_dump($head_parse);
		$body_len   = $head_parse[0];
		echo "Reading response '$body_len'";
		/*
		while ($out = socket_read($socket, 8192)) {
			echo 'out len=:'.strlen($out);
		}
		*/
		echo "closeing socket..\n";
		socket_close($socket);
		echo "ok .\n\n";
	}
}


include_once  "ff/Types.php";

$req   = new ff\echo_thrift_in_t();
$ffrpc = new ffrpc_t('127.0.0.1', 21563);
$ffrpc->call('echo', $req, '', 'ff');
echo "xx" . "bb";
?>
