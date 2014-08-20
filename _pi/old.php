<pre style="font-size: 32px;">
<a href="?p=on">on</a> | <a href="?p=off">off</a> | <a href="?p=g">get</a> | <a href="../">back</a><br />
<?php

/*
The first version, for one device only
*/

if ( isset($_GET['p']) ) {
	$a = $_GET['p'];
	if ( $a == 'on' ) {
		echo 'Response: ' . `./uart 011`;
	} else if ( $a == 'off' ) {
		echo 'Response: ' . `./uart 010`;
	} else if ( $a == 'g' ) {
		echo 'Response: ' . `./uart 00g`;
	}
}

?>
</pre>
