--TEST--
Test for circular references (ZE1)
--SKIPIF--
<?php if(version_compare(zend_version(), "2.0.0-dev", '>=')) echo "skip Zend Engine 1 needed\n"; ?>
--INI--
xdebug.enable=1
xdebug.auto_trace=0
report_memleaks=0
xdebug.collect_params=1
xdebug.auto_profile=0
--FILE--
<?php
	xdebug_start_trace();

	class foo {

		function foo() {
			$this->a = &$this;
			$this->b = &$this;
		}

	}

	function bar($o) {
	}

	$f = new foo();
	bar($f);
	bar($f);

	xdebug_dump_function_trace();
?>
--EXPECTF--
Function trace:
    %f      %d     -> foo->foo() /%s/test14.php:16
    %f      %d     -> bar(class foo {var $a = class foo {var $a = class foo {var $a = ...; var $b = ...}; var $b = class foo {var $a = ...; var $b = ...}}; var $b = class foo {var $a = class foo {var $a = ...; var $b = ...}; var $b = class foo {var $a = ...; var $b = ...}}}) /%s/test14.php:17
    %f      %d     -> bar(class foo {var $a = class foo {var $a = class foo {var $a = ...; var $b = ...}; var $b = class foo {var $a = ...; var $b = ...}}; var $b = class foo {var $a = class foo {var $a = ...; var $b = ...}; var $b = class foo {var $a = ...; var $b = ...}}}) /%s/test14.php:18
