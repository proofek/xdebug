--TEST--
Test for overloaded member functions / classes (ZE2)
--SKIPIF--
<?php if(version_compare(zend_version(), "2.0.0-dev", '<')) echo "skip Zend Engine 2 needed\n"; ?>
--INI--
xdebug.enable=1
xdebug.auto_trace=0
xdebug.auto_profile=0
xdebug.collect_params=1
xdebug.collect_return=0
xdebug.show_mem_delta=0
xdebug.profiler_enable=0
xdebug.trace_format=0
--FILE--
<?php
	$tf = xdebug_start_trace('/tmp/'. uniqid('xdt', TRUE));

	class a {

		function func_a1() {
		}

		function func_a2() {
		}

	}

	class b extends a {

		function func_b1() {
		}

		function func_b2() {
		}

	}

	$B = new b;
	$B->func_a1();
	$B->func_b1();

	echo file_get_contents($tf);
	unlink($tf);
?>
--EXPECTF--
TRACE START [%d-%d-%d %d:%d:%d]
    %f      %d     -> a->func_a1() /%s/test16b.php:25
    %f      %d     -> b->func_b1() /%s/test16b.php:26
    %f      %d     -> file_get_contents('/tmp/%s') /%s/test16b.php:28
