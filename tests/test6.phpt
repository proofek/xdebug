--TEST--
Test for complex parameters to a function
--INI--
xdebug.enable=1
--FILE--
<?php
	function foo2 ($a, $b, $c)
	{
		return foo();
	}

	foo2 (4, array(array('blaat', 5, FALSE)));
?>
--EXPECTF--
Warning: Missing argument 3 for foo2()
 in /%s/test6.php on line 2

Call Stack:
    %f      %d   1. {main}() /%s/test6.php:0
    %f      %d   2. foo2(4, array (0 => array (0 => 'blaat', 1 => 5, 2 => FALSE))) /%s/test6.php:7

Fatal error: Call to undefined function:  foo() in /%s/test6.php on line 4

Call Stack:
    %f      %d   1. {main}() /%s/test6.php:0
    %f      %d   2. foo2(4, array (0 => array (0 => 'blaat', 1 => 5, 2 => FALSE))) /%s/test6.php:7
