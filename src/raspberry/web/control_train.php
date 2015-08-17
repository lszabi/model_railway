<?php

if ( !isset($_GET['id']) || !is_numeric($_GET['id']) || $_GET['id'] < 0 || $_GET['id'] > 255 ) {
	die('Invalid id. <a href="index.php">back</a>');
}

$id = $_GET['id'];

$id_str = substr('00' . dechex($id), -2);

?>
<html>

<head>
<script type="text/javascript" src="script.js"></script>
<link rel="stylesheet" href="style.css" />
</head>

<body>
<div align="center">
<table id="main_table" cellspacing="0">
<tr><td class="cent" colspan="3"><h1>Train Control Panel</h1></td></tr>
<tr><td rowspan="4" class="control_canvas"><canvas id="main_canvas" width="300" height="300"></canvas></td>
<td colspan="2" class="cent">ID: <?php echo $id_str; ?></td></tr>
<td colspan="2" class="cent"><input type="range" style="width: 100%" min="0" max="255" value="0" step="1" id="g_val" oninput="motor_change();" /></td></tr>
<tr class="cent"><td id="dirtd_a" onclick="dir_click(0);">Direction A</td><td id="dirtd_b" onclick="dir_click(1);">Direction B</td></tr>
<tr class="cent"><td id="lights_td" onclick="turn_lights();">Lights</td>
<td onmouseover="this.style.backgroundColor = 'red';" onmouseout="this.style.backgroundColor = 'white';" onclick="motor_stop();" onmouseup="this.style.backgroundColor = 'white';">STOP</td></tr>
</table>
<pre id="log"></pre>
</div>
<script type="text/javascript">
var train_id = '<?php echo $id_str; ?>';
var ctx = $('main_canvas').getContext("2d");

ws_connect('ws://<?php echo $_SERVER['SERVER_NAME']; ?>:9090');

refresh_gauge();
</script>
</body>

</html>