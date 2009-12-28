--TEST--
Test for tracing mixed array element and property assignments in user-readable function traces (< PHP 5.3)
--SKIPIF--
<?php if(version_compare(phpversion(), "5.3.0", '>=')) echo "skip < PHP 5.3 needed\n"; ?>
--INI--
xdebug.default_enable=1
xdebug.profiler_enable=0
xdebug.auto_trace=0
xdebug.trace_format=0
xdebug.collect_vars=1
xdebug.collect_params=4
xdebug.collect_returns=0
xdebug.collect_assignments=1
xdebug.var_display_max_depth=9
--FILE--
<?php
$tf = xdebug_start_trace('/tmp/'. uniqid('xdt', TRUE));

class testClass
{
	public $a;
	static public $b;

	function __construct( $obj )
	{
		$obj->a = array();
		$obj->a['bar'] = 52;
		$obj->a['foo'] = new StdClass;
		$obj->a['foo']->bar = 52;

		$this->a = array();
		$this->a['bar'] = 52;
		$this->a['foo'] = new StdClass;
		$this->a['foo']->bar = 52;

		self::$b = array();
		self::$b['bar'] = array();
		self::$b['foo'] = new StdClass;
		self::$b['foo']->bar = 52;
	}
}

$a = new testClass( new StdClass );

xdebug_stop_trace();
echo file_get_contents($tf);
unlink($tf);
?>
--EXPECTF--
TRACE START [%d-%d-%d %d:%d:%d]
                         => $tf = '/tmp/%s.xt' %sassignment-trace9-php52.php:2
%w%f %w%d     -> testClass->__construct($obj = class stdClass {  }) %sassignment-trace9-php52.php:28
                           => $obj->a = array () %sassignment-trace9-php52.php:11
                           => $obj->a['bar'] = 52 %sassignment-trace9-php52.php:12
                           => $obj->a['foo'] = class stdClass {  } %sassignment-trace9-php52.php:13
                           => $obj->a['foo']->bar = 52 %sassignment-trace9-php52.php:14
                           => $this->a = array () %sassignment-trace9-php52.php:16
                           => $this->a['bar'] = 52 %sassignment-trace9-php52.php:17
                           => $this->a['foo'] = class stdClass {  } %sassignment-trace9-php52.php:18
                           => $this->a['foo']->bar = 52 %sassignment-trace9-php52.php:19
                           => self::b = array () %sassignment-trace9-php52.php:21
                           => self::b['bar'] = array () %sassignment-trace9-php52.php:22
                           => self::b['foo'] = class stdClass {  } %sassignment-trace9-php52.php:23
                           => b['foo']->bar = 52 %sassignment-trace9-php52.php:24
                         => $a = class testClass { public $a = array ('bar' => 52, 'foo' => class stdClass { public $bar = 52 }) } %sassignment-trace9-php52.php:28
%w%f %w%d     -> xdebug_stop_trace() %sassignment-trace9-php52.php:30
%w%f %w%d
TRACE END   [%d-%d-%d %d:%d:%d]
