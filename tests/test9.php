<?php
xdebug_start_trace();
class DBHelper
{
  function quote($s) {
  }
}

class DB
{
  function query($s) {
  }
}

$db = new DB;

$db->query("insert blah '".DBHelper::quote("test's").DBHelper::quote("test's")."' blah");
//$db->query("insert blah ' blah");
xdebug_get_function_trace();
?>
