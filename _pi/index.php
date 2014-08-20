<pre style="font-size: 32px;">
<a href="?">refresh</a> | <a href="?a=c&id=0">connect</a> | <a href="?a=r&id=0">cancel connect</a> | <a href="?a=l&id=0">get log</a> | <a href="../">back</a><br />
<?php

/*
Model railway controller
created by LSzabi

uses RaspberryPi to connect to Arduino MEGA via UART
see main.c

Don't forget: sudo chmod a+rw /dev/ttyAMA0
*/

$list = `./uart 00g`; // list of devices

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

if ( isset($_GET['a']) && isset($_GET['id']) ) { // take action
	$id = $_GET['id'];
	$a = $_GET['a'];
	if ( is_numeric($id) ) {
		$id = strtoupper(dechex($id));
		while ( strlen($id) < 2 ) {
			$id = '0' . $id; // avr int-hex conversion function operates only with upper-case
		}
		if ( $a == 'on' ) {
			$ret = shell_exec('./uart ' . $id . '1');
		} else if ( $a == 'off' ) {
			$ret = shell_exec('./uart ' . $id . '0');
		} else if ( $a == 'c' ) {
			$ret = `./uart 00c`;
		} else if ( $a == 'r' ) {
			$ret = `./uart 00r`;
		} else if ( $a == 'l' ) {
			$ret = `./uart 00l`;
		}
	}
}

if ( $ret != '' ) {
	echo 'Response: ' . $ret . "\n\n"; // print response, if there's any (if not, it's probably an error)
}

foreach ( $ids as $id ) {
	echo "#$id:\t<a href=\"?a=on&id=$id\">on</a> | <a href=\"?a=off&id=$id\">off</a>\n"; // enum devices
}

echo "all:\t<a href=\"?a=on&id=255\">on</a> | <a href=\"?a=off&id=255\">off</a>\n"; // broadcast message

?>
</pre>
