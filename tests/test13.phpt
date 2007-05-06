--TEST--
Test for variable function calls
--SKIPIF--
<?php if (!extension_loaded("xdebug")) print "skip"; ?>
--INI--
xdebug.enable=1
xdebug.auto_trace=0
xdebug.collect_params=3
xdebug.collect_return=0
xdebug.auto_profile=0
xdebug.profiler_enable=0
xdebug.show_mem_delta=0
xdebug.trace_format=0
--FILE--
<?php
	$tf = xdebug_start_trace('/tmp/'. uniqid('xdt', TRUE));

	function foo1 ($a)
	{
		return addslashes ($a);
	}

	function foo2 ($a)
	{
		return addslashes ($a);
	}

	function foo3 ($a)
	{
		return addslashes ($a);
	}

	function foo4 ($a)
	{
		return addslashes ($a);
	}

	$f = 'foo1';
	$f('test\'s');
	$g = 'foo4';
	$g('test\'s');
	$h = 'foo2';
	$h('test\'s');
	$i = 'foo3';
	$i('test\'s');

	echo file_get_contents($tf);
	unlink($tf);
?>
--EXPECTF--
TRACE START [%d-%d-%d %d:%d:%d]
%w%f %w%d     -> foo1('test\'s') /%s/test13.php:25
%w%f %w%d       -> addslashes('test\'s') /%s/test13.php:6
%w%f %w%d     -> foo4('test\'s') /%s/test13.php:27
%w%f %w%d       -> addslashes('test\'s') /%s/test13.php:21
%w%f %w%d     -> foo2('test\'s') /%s/test13.php:29
%w%f %w%d       -> addslashes('test\'s') /%s/test13.php:11
%w%f %w%d     -> foo3('test\'s') /%s/test13.php:31
%w%f %w%d       -> addslashes('test\'s') /%s/test13.php:16
%w%f %w%d     -> file_get_contents('/tmp/%s') /%s/test13.php:33
