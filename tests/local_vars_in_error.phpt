--TEST--
Test with showing variables in the top most stackframe on errors
--INI--
xdebug.enable=1
xdebug.auto_trace=0
xdebug.collect_params=1
xdebug.auto_profile=0
xdebug.dump_globals=0
--FILE--
<?php
	function a($a,$b) {
		$c = array($a, $b * $b);
		$d = new stdClass;
		do_f();
	}

	a(5, 6);
?>
--EXPECTF--
Fatal error: Call to undefined function do_f() in /%s/local_vars_in_error.php on line 5

Call Stack:
    0.0003      37480   1. {main}() /%s/local_vars_in_error.php:0
    0.0004      37480   2. a(5, 6) /%s/local_vars_in_error.php:8


Variables in local scope:
  $d = class stdClass {}
  $a = 5
  $c = array (0 => 5, 1 => 36)
  $b = 6
