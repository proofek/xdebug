--TEST--
Test for simple backtrace
--INI--
xdebug.enable=1
--FILE--
<?php
	function a () {
		b();
	}

	function b () {
		c();
	}

	a();
?>
--EXPECTF--
Fatal error: Call to undefined function:  c() in /%s/test4.php on line 7

Call Stack:
    %f      %d   1. {main}() /%s/test4.php:0
    %f      %d   2. a() /%s/test4.php:10
    %f      %d   3. b() /%s/test4.php:3
