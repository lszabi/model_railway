/*
Packet format: cmd + args (1 byte hex each)
Commads:
l[uid][state] - set light
m[uid][dir][speed] - set motor speed
s[uid1][uid2][state] - state: 0 - straight, 1 - fork
*/

var ctx = -1;
var con = -1;

var motor_direction = 0;
var train_lights = false;
var train_id = -1;

var switch_pos = 0;
var switch_id = -1;

function $(id) {
	return document.getElementById(id);
}

function ws_connect(url) {
	con = new WebSocket(url);
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

function draw_gauge(ctx, x, y, r, val) {
	if ( val > 255 || val < 0 ) {
		val = 0;
	}
	ctx.clearRect(0, 0, 300, 300);
	// Scales
	ctx.strokeStyle = 'blue';
	ctx.lineWidth = r / 25;
	for ( var i = 0; i <= 256; i += 16 ) {
		var alpha = Math.PI * ( 0.85 + 1.3 * ( i / 255 ) );
		ctx.beginPath();
		ctx.moveTo(x + 0.9 * r * Math.cos(alpha), y + 0.9 * r * Math.sin(alpha));
		ctx.lineTo(x + r * Math.cos(alpha), y + r * Math.sin(alpha));
		ctx.stroke();
	}
	// Outer circle
	ctx.lineWidth = r / 12;
	ctx.beginPath();
	ctx.arc(x, y, r, 0.8 * Math.PI, 0.2 * Math.PI);
	ctx.stroke();
	// Inner circle
	ctx.beginPath();
	ctx.fillStyle = 'red';
	ctx.arc(x, y, r / 10, 0, 2 * Math.PI);
	ctx.fill();
	// Pointer
	ctx.beginPath();
	ctx.moveTo(x, y);
	ctx.strokeStyle = 'red';
	ctx.lineWidth = r / 20;
	var alpha = Math.PI * ( 0.85 + 1.3 * ( val / 255 ) );
	ctx.lineTo(x + 0.8 * r * Math.cos(alpha), y + 0.8 * r * Math.sin(alpha));
	ctx.stroke();
	// Text
	ctx.textAlign = 'center';
	ctx.textBaseline = 'bottom';
	ctx.fillStyle = 'green';
	ctx.font = ( r / 4 ) + 'px Arial';
	ctx.fillText(val, x, y + 0.6 * r);
}

function refresh_gauge() {
	draw_gauge(ctx, 150, 150, 120, $('g_val').value);
}

function motor_change() {
	var value = parseInt($('g_val').value);
	if ( motor_direction ) {
		ws_send('m' + train_id + '01' + ( value <= 0xF ? '0' : '' ) + value.toString(16));
	} else {
		ws_send('m' + train_id + '00' + ( value <= 0xF ? '0' : '' ) + value.toString(16));
	}
	refresh_gauge();
}

function motor_stop() {
	$('g_val').value = 0;
	motor_change();
}

function dir_click(dir) {
	if ( dir == 0 ) {
		$('dirtd_a').style.backgroundColor = 'lime';
		$('dirtd_b').style.backgroundColor = 'white';
	} else {
		$('dirtd_b').style.backgroundColor = 'lime';
		$('dirtd_a').style.backgroundColor = 'white';
	}
	motor_direction = dir;
	motor_stop();
}

function turn_lights() {
	train_lights = !train_lights;
	if ( train_lights ) {
		$('lights_td').style.backgroundColor = 'blue';
		$('lights_td').style.color = 'white';
		ws_send('l' + train_id + '01');
	} else {
		$('lights_td').style.backgroundColor = 'white';
		$('lights_td').style.color = 'black';
		ws_send('l' + train_id + '00');
	}
}

function switch_click(pos) {
	if ( pos == 0 ) {
		$('switchtd_a').style.backgroundColor = 'lime';
		$('switchtd_b').style.backgroundColor = 'white';
		ws_send('s' + switch_id + '0');
	} else {
		$('switchtd_a').style.backgroundColor = 'white';
		$('switchtd_b').style.backgroundColor = 'lime';
		ws_send('s' + switch_id + '1');
	}
	switch_pos = pos;
	refresh_switch();
}

function draw_switch(x, y, size) {
	ctx.lineWidth = size / 50;
	var draw_rails = [ function() {
		// Straight rails
		ctx.beginPath();
		ctx.moveTo(x - size * 0.125, y - size * 0.5);
		ctx.lineTo(x - size * 0.125, y + size * 0.5);
		ctx.stroke();
		ctx.beginPath();
		ctx.moveTo(x - size * 0.375, y - size * 0.5);
		ctx.lineTo(x - size * 0.375, y + size * 0.5);
		ctx.stroke();
	}, function() {
		// Curved rails
		ctx.beginPath();
		ctx.moveTo(x - size * 0.125, y + size * 0.5);
		ctx.bezierCurveTo(x - size * 0.125, y, x + size * 0.375, y, x + size * 0.375, y - size * 0.5);
		ctx.stroke();
		ctx.beginPath();
		ctx.moveTo(x - size * 0.375, y + size * 0.5);
		ctx.bezierCurveTo(x - size * 0.375, y - size * 0.125, x + size * 0.125, y - size * 0.125, x + size * 0.125, y - size * 0.5);
		ctx.stroke();
	} ];
	ctx.strokeStyle = 'black';
	draw_rails[1 - switch_pos]();
	ctx.strokeStyle = 'red';
	draw_rails[switch_pos]();
}

function refresh_switch() {
	draw_switch(150, 150, 250);
}