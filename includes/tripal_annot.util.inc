<?php
#
# Utility functions
#
function tripal_annot_build_blast_output_path($jobkey,$dbnum)
{
	$name = "blast_$jobkey"."_$dbnum";
	return tripal_annot_build_file_path($name);
}
function tripal_annot_dump_object($title,$obj)
{
	$text = print_r($obj,TRUE);
	tripal_annot_log_error("---------------------\n$title\n$text\n---------------\n");
}
function tripal_annot_clear_errlog()
{
	$path = variable_get('annot_elog', '');
	file_put_contents($path,"");
}
#
# Log to the error log file
# Errors including path information go here, not seen by user
#
function tripal_annot_log_error($err)
{
	$path = variable_get('annot_elog', '');
	if (substr($err,-1) != "\n")
	{
		$err .= "\n";
	}
	$ret =  file_put_contents($path,$err,FILE_APPEND);
}
function tripal_annot_file_dir()
{
	$dirname = variable_get("annot_files_dir");
	return DRUPAL_ROOT."/".tripal_get_files_dir($dirname); # throws an error if doesn't exist
}
function tripal_annot_build_file_path($filename)
{
	return tripal_annot_file_dir()."/$filename";
}
function tripal_annot_get_job_info($jid,$jobkey)
{
	if ($jid != 0)
	{
		$res = db_query("select * from tripal_annot where jid=$jid");
		if ($res)
		{
			$r = $res->fetchObject();			
			return $r;
		}
	}
	else if ($jobkey != "")
	{
		$res = db_query("select * from tripal_annot where jobkey='$jobkey'");
		if ($res)
		{
			$r = $res->fetchObject();			
			return $r;
		}
	}
	return FALSE;
}
# Check or attempt to create the files dir specified in config. 
# This will be done during config, however if a system is cloned or mirrored
# then it is possible for things like /tmp subdirectories to be missing.
#
function tripal_annot_check_file_dir()
{
	$dirname = variable_get("annot_files_dir");
	$dir = DRUPAL_ROOT."/".tripal_get_files_dir()."/$dirname";
	if (is_dir($dir))
	{
		return 1;
	}
	else
	{
		if (is_link($dir) )
		{
			$realpath = readlink($dir);
			mkdir($realpath,755);
			system("chmod 755 $realpath");
		}
		else
		{
			mkdir($dir,755);
			system("chmod 755 $dir");
		}
	}
	if (is_dir($dir))
	{
		return 1;
	}
	return 0;
}

function tripal_annot_update_field($jid,$field,$val)
{
	db_update('tripal_annot')
		->fields(array(
	        "$field" => $val
	        )
		)
		->condition('jid', $jid)
		->execute();
	
}
function path_from_fid($fid)
{
	$fobj = file_load($fid);
	return drupal_realpath($fobj->uri);
}
function tripal_annot_test_errlog()
{
	$path = variable_get('annot_elog', '');
	if(!empty($path)) 
	{
		if(file_exists($path) ) 
		{
			system("chmod 755 $path");
			return 1;
		}
		else
		{
			file_put_contents($path,"Annot error log");
			if(file_exists($path) ) 
			{
				system("chmod 755 $path");
				return 1;
			}
		}
	}
	return 0;
}
function tripal_annot_mail($key, &$message, $params) 
{
      $message['subject'] = "LIS Annotation Job";
      $message['body'][] = $params['msg'];
}
