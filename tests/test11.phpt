--TEST--
Test for indirect function call
--INI--
xdebug.enable=1
xdebug.auto_trace=0
--FILE--
<?php
	xdebug_start_trace();

	function blaat ()
	{
	}

	$func = 'blaat';
	echo $func();

	xdebug_dump_function_trace();
?>
--EXPECTF--
Function trace:
    %f      %d     -> blaat() /%s/phpt.%x:9
