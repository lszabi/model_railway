<?php

if ( !isset($_GET['id']) || !is_numeric($_GET['id']) || $_GET['id'] < 0 || $_GET['id'] > 255 ) {
	die('Invalid id. <a href="index.php">back</a>');
}

$id = $_GET['id'];

$id_str = substr('0000' . dechex($id), -4);

?>
<html>

<head>
<script type="text/javascript" src="script.js"></script>
<link rel="stylesheet" href="style.css" />
</head>

<body>
<div align="center">
<table id="main_table" cellspacing="0">
<tr><td class="cent" colspan="3"><h1>Switch Control Panel</h1></td></tr>
<tr><td rowspan="3" class="control_canvas"><canvas id="main_canvas" width="300" height="300"></canvas></td>
<td class="cent">ID: <?php echo $id_str; ?></td></tr>
<tr><td id="switchtd_a" onclick="switch_click(0);" class="cent">Straight</td></tr>
<tr><td id="switchtd_b" onclick="switch_click(1);" class="cent">Fork</td></tr>
</table>
<pre id="log"></pre>
</div>
<script type="text/javascript">
var switch_id = '<?php echo $id_str; ?>';
var ctx = $('main_canvas').getContext("2d");

ws_connect('ws://<?php echo $_SERVER['SERVER_NAME']; ?>:9090');

$('switchtd_a').style.backgroundColor = 'lime';
refresh_switch();
</script>
</body>

</html>