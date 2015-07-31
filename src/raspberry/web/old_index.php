<pre style="font-size: 40px;">
<a href="?">refresh</a> | <a href="?a=c&id=0">connect</a> | <a href="?a=r&id=0">cancel connect</a>
<a href="?a=l&id=0">get log</a> | <a href="../">back</a><br />
<?php

/*
Model railway controller
created by LSzabi

uses RaspberryPi to connect to Arduino MEGA via UART
see main.c

Don't forget: sudo chmod a+rw /dev/ttyAMA0
*/

function format_id($id) {
	$id = strtoupper(dechex($id));
	while ( strlen($id) < 2 ) {
		$id = '0' . $id;
	}
	return $id;
}

function send_cmd($id, $c) {
	return shell_exec('./uart ' . $id . $c);
}

$list = `./uart 00g`;

$bytes = explode(',', $list);

if ( count($bytes) < 8 ) {
	die("Error: list = $list");
}

$ids = array();

for ( $i = 0; $i < 8; $i++ ) {
	$b = hexdec($bytes[$i]);
	for ( $f = 0; $f < 32; $f++ ) {
		if ( $b & ( 1 << $f ) ) {
			$ids[] = $i * 32 + $f + 1;
		}
	}
}

$ret = '';

if ( isset($_GET['a']) && isset($_GET['id']) ) {
	$id = $_GET['id'];
	$a = $_GET['a'];
	if ( is_numeric($id) ) {
		$id = format_id($id);
		if ( $a == 'on' ) {
			$ret = send_cmd($id, '1');
		} else if ( $a == 'off' ) {
			$ret = send_cmd($id, '0');
		} else if ( $a == 'c' ) {
			$ret = send_cmd(0, 'c');
		} else if ( $a == 'r' ) {
			$ret = send_cmd(0, 'r');
		} else if ( $a == 'l' ) {
			$ret = send_cmd(0, 'l');
		} else if ( $a == 's' ) {
			$ret = send_cmd($id, 's');
		} else if ( $a == 'a' ) {
			$ret = send_cmd($id, 'a');
		} else if ( $a == 'b' ) {
			$ret = send_cmd($id, 'b');
		}
	}
}

if ( $ret != '' ) {
	echo 'Response: ' . $ret . "\n\n";
}

foreach ( $ids as $id ) {
	$hex_id = format_id($id);
	$uid = shell_exec('./uart ' . $hex_id . 'i');
	echo "#$id:\t$uid\t<a href=\"?a=on&id=$id\">on</a> | <a href=\"?a=off&id=$id\">off</a> | <a href=\"?a=s&id=$id\">stop</a> | <a href=\"?a=a&id=$id\">a</a> | <a href=\"?a=b&id=$id\">b</a>\n";
}

echo "all:\t\t<a href=\"?a=on&id=255\">on</a> | <a href=\"?a=off&id=255\">off</a> | <a href=\"?a=s&id=255\">stop</a> | <a href=\"?a=a&id=255\">a</a> | <a href=\"?a=b&id=255\">b</a>\n";

?>
</pre>
