--TEST--
Test for traces to file
--INI--
xdebug.enable=1
xdebug.auto_trace=0
xdebug.collect_params=1
xdebug.auto_profile=0
--FILE--
<?php
	require 'bug00002.inc';

	$action = 'do_stuff';
	$tf = xdebug_start_trace('/tmp/bug00002.trace');
	$action();
	xdebug_stop_trace();
	readfile($tf);
	unlink($tf);
?>
--EXPECTF--

TRACE START [%d-%d-%d %d:%d:%d]
    %f      %d     -> do_stuff() /%s/bug00002.php:6
    %f      %d     -> xdebug_stop_trace() /%s/bug00002.php:7
TRACE END   [%d-%d-%d %d:%d:%d]
