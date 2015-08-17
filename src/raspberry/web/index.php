<html>

<head>
<title>Model Railway</title>
<link rel="stylesheet" href="style.css" />
</head>

<body>
<div align="center">
<h1>Model Railway devices</h1>
<table id="main_table" cellspacing="0" class="devlist">
<tr style="font-weight: bold;"><td>ID</td><td>type</td><td>status</td><td>override</td></tr>
<?php

$devices = explode(',', file_get_contents('devlist.txt'));

$dev_types = array('train', 'switch');

foreach ( $devices as $dev ) {
	$type = ( $dev & 0xFF0000 ) >> 16;
	$id = $dev & 0xFFFF;
	echo '<tr><td>' . substr('0000' . dechex($id), -4) . '</td><td>' . $dev_types[$type] . '</td><td>n/a</td><td><a href="control_' . $dev_types[$type] . '.php?id=' . $id . '">override</a></td></tr>';
}

?>
</table>
</div>
</body>

</html>