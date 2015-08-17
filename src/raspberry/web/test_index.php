<html>

<head>
<title>Model Railway Control Station</title>
<style>
button {
	width: 100px;
	height: 100px;
}
</style>
</head>

<body onload="ws_connect();">
<script type="text/javascript">
/*
Packet format: cmd + args (1 byte hex each)
Commads:
l[uid][state] - set light
m[uid][dir][speed] - set motor speed
s[uid][status] - get status: 00: states, 01: speed
*/

var con = -1;
var timeout_id = -1;

function $(id) {
	return document.getElementById(id);
}

function ws_connect() {
	con = new WebSocket('ws://<?php echo $_SERVER['SERVER_NAME']; ?>:9090');
	con.onmessage = function(evt) {
		$('log').innerText += evt.data + "\n";
	}
	con.onerror = function() {
		alert('Error in websocket connection');
	}
}

function ws_send(a) {
	if ( con != -1 ) {
		while ( con.readyState != 1 );
		con.send(a);
	}
}

function ws_motor() {
	clearTimeout(timeout_id);
	$('motor_text').innerHTML = $('motor_value').value.toString();
	timeout_id = setTimeout(ws_motor_send, 100);
}

function ws_motor_send() {
	var value = $('motor_value').value;
	ws_send('mAA' + ( value <= 0 ? '00' : '01' ) + ( Math.abs(value) <= 0xF ? '0' : '' ) + Math.abs(value).toString(16));
}

function ws_motor_stop() {
	$('motor_value').value = 0;
	$('motor_text').innerHTML = '0';
	ws_motor_send();
}
</script>
<button onclick="ws_send('lAA01');">On</button> | <button onclick="ws_send('lAA00');">Off</button> | 
<button onclick="ws_motor_stop();">Stop</button> | <button onclick="ws_send('sAA00'); setTimeout(function(){ ws_send('sAA01'); }, 500);">Status</button><br />
<input type="range" style="width: 450px" id="motor_value" oninput="ws_motor();" min="-255" max="255" step="5" value="0" />
<span id="motor_text">0</span>
<pre id="log"></pre>
</body>

</html>