--TEST--
Test for bug #146: Array key names with quotes in traces are not escaped
--SKIPIF--
<?php if (!extension_loaded("xdebug")) print "skip"; ?>
--INI--
xdebug.enable=1
xdebug.auto_trace=0
xdebug.collect_params=1
xdebug.collect_return=1
xdebug.auto_profile=0
xdebug.profiler_enable=0
xdebug.show_mem_delta=0
xdebug.trace_format=0
--FILE--
<?php
	$tf = xdebug_start_trace('/tmp/'. uniqid('xdt', TRUE));

	function foo($a)
	{
		return $a;
	}

	$array = array("te\"st's" => 42);
	foo($array);

	echo file_get_contents($tf);
	unlink($tf);
?>
--EXPECTF--
TRACE START [%d-%d-%d %d:%d:%d]
%w%f %w%d     -> foo(array ('te"st\'s' => 42)) /%s/bug00146.php:10
                           >=> array ('te"st\'s' => 42)
%w%f %w%d     -> file_get_contents('/tmp/%s') /%s/bug00146.php:12
