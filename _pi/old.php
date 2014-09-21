<pre style="font-size: 32px;">
<a href="?p=on">on</a> | <a href="?p=off">off</a> | <a href="?p=g">get</a> | <a href="../">back</a><br />
<?php

if ( isset($_GET['p']) ) {
	$a = $_GET['p'];
	if ( $a == 'on' ) {
		echo 'Response: ' . `./uart 1`;
	} else if ( $a == 'off' ) {
		echo 'Response: ' . `./uart 0`;
	} else if ( $a == 'g' ) {
		echo 'Response: ' . `./uart g`;
	}
}

?>
</pre>
