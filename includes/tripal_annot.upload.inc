<?php
error_reporting(E_STRICT);

#####################################################################################
#
# Upload file page: Landing page after upload
#					File is cleaned (bad chars replaced by N) 
#					The cleaned file is stored but does not replace the original
#					(This page can be revisited and additional jobs started on the same upload)
# 					User starts job from here	
#					Append ?debug=1  to get extra debug information for upload parse problems
#
#######################################################################################

function annot_upload_page($fid,$init='')
{
	global $ta_debug;
	global $debug_text;
	$debug_text = "";
	$ta_debug = (isset($_GET["debug"]) ? 1 : 0);
	$seqType = $_GET["seqtype"];
	if ($seqType != "n" && $seqType != "p")
	{
		return "Sequence type was not provided!<p>";
	}
	$fobj = file_load($fid);
	$upload_time = $fobj->timestamp;
	$cur_time = time();
	if (($cur_time - $upload_time) > 60*60) 
	{
		$text = <<<END
This file upload has expired. Please upload your file again. 
END;
		return $text;
	}
	$fpath = drupal_realpath($fobj->uri);
	$fname = $fobj->filename;
	$fsize = $fobj->filesize;
	$text = "Uploaded file: $fname ($fsize bytes)<p>";
	system("chmod 755 $fpath");
	$fh = fopen($fpath,"r");

	# Clean the file of unusable chars and also make sure the seqs are all nonempty
	# The cleaned file is not used for anything until the user clicks to start the analysis

	$tmppath = "$fpath.cleaned";
	$fhtemp = fopen($tmppath,"w");

	$curName = "";
	$curHeader = "";
	$curSeq = "";
	$numSeqs = 0;
	$totalLen = 0;
	$zeroLenSeqs = array();
	$totalBadChars = 0;
	$badCharsBySeq = array();
	$wrongTypeSeqs = array();  # seqs that look like nt although prot was specified, or vice versa

	while (($line = fgets($fh)) !== false)
	{
		if (preg_match('/^>(\S+)(\s.*)/',$line,$matches))
		{
			if ($curName != "")
			{
				$numSeqs++;
				if (strlen($curSeq) > 0)
				{
					$totalLen += strlen($curSeq);
					$numBadChars = 0;
					check_seq_type($curSeq,$curName,$seqType,$wrongTypeSeqs);
					$cleanedSeq = clean_sequence($curSeq,$seqType,$numBadChars);
					$totalBadChars += $numBadChars;
					$badCharsBySeq[$curName] = $numBadChars;
					write_cleaned_sequence($fhtemp,$curName,$curHeader,$cleanedSeq);
				}
				else
				{
					array_push($zeroLenSeqs,$curName);		
				}
			}
			$curName = $matches[1];
			$curHeader = preg_replace("/\s*$/","",$matches[2]); # rest of header, minus newlines, ^M, at end
																# in fact we don't use header
			$curSeq = "";
		}
		else
		{
			$curSeq .= trim(strtolower($line));
		}
	}
	if ($curName != "")
	{
		$numSeqs++;
		if (strlen($curSeq) > 0)
		{
			$totalLen += strlen($curSeq);
			$numBadChars = 0;
			check_seq_type($curSeq,$curName,$seqType,$wrongTypeSeqs);
			$cleanedSeq = clean_sequence($curSeq,$seqType,$numBadChars);
			$totalBadChars += $numBadChars;
			$badCharsBySeq[$curName] = $numBadChars;
			write_cleaned_sequence($fhtemp,$curName,$curHeader,$cleanedSeq);
		}
		else
		{
			array_push($zeroLenSeqs,$curName);		
		}
	}

	fclose($fh);
	fclose($fhtemp);


	if ($numSeqs == 0 || $totalLen == 0)
	{
		$text .= <<<END
<p>
<p>
No sequences were found in the file.
<p>
Please make sure that the file is a plain-text file, i.e., not a Word
<br>
document, rich-text file, or other file containing formatting characters. 
<p>
Also make sure that the file is in the FASTA format.
END;
		return $text;
	}
	else if (count($zeroLenSeqs) > 0 )
	{
		$text .= "<hr><br>The file cannot be used because it contains the following empty sequences:<br>";
		$text .= implode("<br>",$zeroLenSeqs);
		return $text;
	}
	else
	{
		$st = ($seqType == "n" ? "nucleotide" : "protein");
		$nsymbol = ($seqType == "n" ? "n" : "x");
		$goodchars = ($seqType == "n" ? "agctn" : "abcdefghiklmnpqrstuvwxyz");
		$text .= <<<END
Sequence type: $st<br>
Number of sequences: $numSeqs <br>
Total sequence length: $totalLen<br>
END;
		if ($totalBadChars > 0)
		{
			$text .= <<<END
$totalBadChars invalid symbols were found 
(symbols other than $goodchars)
<br>
These symbols will be changed to $nsymbol
END;
		}
		if ($seqType == "p" && count($wrongTypeSeqs) > 0)
		{
			$seqList = implode("<br>\n",$wrongTypeSeqs);
			$text .= <<<END
<p><b>Warning:</b><br>
The following sequences do not look like protein sequences. Please verify that they <br>
are correct protein sequences, or 
<a href="/annot" style="text-decoration:underline">start over</a>:
<br> $seqList <p>
END;
		}
		if ($seqType == "n" && count($wrongTypeSeqs) > 0)
		{
			$seqList = implode("<br>\n",$wrongTypeSeqs);
			$text .= <<<END
<p><b>Warning:</b><br>
The following sequences do not look like nucleotide sequences. Please verify that they <br>
are correct nucleotide sequences, or 
<a href="/annot" style="text-decoration:underline">start over</a>:
<br> $seqList <p>
END;
		}
		$text .= <<<END
<p>
<b>Please verify that this is correct before starting your job</b> 
(or, 
<a href="/annot" style="text-decoration:underline">start over</a>)
<p>
END;
		$text .= drupal_render(drupal_get_form('annot_upload_form', 
									$fid, $seqType, $numSeqs, $totalLen)); 
		
	}
	return $text;
}
function check_seq_type($seq, $name, $seqtype, &$wrongArray)
{
	$test = preg_replace('/[agctn]/','',$seq);
	if (strlen($test) < strlen($seq)/3)
	{
		# nucleotide
		if ($seqtype == "p")
		{
			array_push($wrongArray,$name);
		}	
	}
	else
	{
		if ($seqtype == "n")
		{
			array_push($wrongArray,$name);
		}
	}
}
function clean_sequence($seq,$seqtype,&$badchars)
{
	$cleaned = "";
	if ($seqtype == "n")
	{
		$cleaned = preg_replace('/[^agctn]/','n',$seq,-1,$badchars);
	}	
	else
	{
		$cleaned = preg_replace('/[^abcdefghiklmnpqrstuvwxyz]/','x',$seq,-1,$badchars);
	}
	return $cleaned;
}
function write_cleaned_sequence($fh,$name,$header,$seq)
{
	$name = preg_replace("/;$/","",$name); # remove trailing semi if any since Blast will remove it
	fwrite($fh,">$name\n"); # Just leave off the header...plays no role here
	for ($i = 0; $i < strlen($seq); $i += 60)
	{
		fwrite($fh,substr($seq,$i,60)."\n");
	}
}
function annot_upload_form($form,&$form_state)
{
	$fid = $form_state['build_info']['args'][0];
	$type = $form_state['build_info']['args'][1];
	$nseqs = $form_state['build_info']['args'][2];
	$totlen = $form_state['build_info']['args'][3];
	$use_email = variable_get("annot_use_email");

    $form = array();

	if ($use_email)
	{
		$form['email'] = array(
			'#type' => 'textfield',
			'#title' => "Email (if you wish to be notified):",
			'#size' => 25
		);
	}

	$form['fid'] = array(
		'#type' => 'hidden',
		'#value' => $fid,
	);
	$form['type'] = array(
		'#type' => 'hidden',
		'#value' => $type,
	);
	$form['nseqs'] = array(
		'#type' => 'hidden',
		'#value' => $nseqs,
	);
	$form['totlen'] = array(
		'#type' => 'hidden',
		'#value' => $totlen,
	);
	$form['submit'] = array(
		'#type' => 'submit',
		'#value' => 'Begin Annotation',
	  );

	return $form;
}
# Validating that the file is still there is probably excessive.
function annot_upload_form_validate($form, &$form_state)
{
  	$fid = $form_state['values']['fid'];

	$fobj = file_load($fid);
	$fpath = drupal_realpath($fobj->uri);
	if (!file_exists($fpath))
	{
			form_set_error('fid', t('The uploaded file is no longer available! Please re-upload.'));
	}
}

# 
# User has hit the "submit job" button to launch the job
# Note that the file has already been analyzed and cleaned by this point
#
# Steps:
# Create job key string
# Make DB entry
# Queue tripal job
# Email notify
# Redirect to tracking page
#
function annot_upload_form_submit($form, &$form_state)
{
	global $user;

	#annot_update_schema();
	$use_email = variable_get("annot_use_email");
  	$email = ($use_email ? $form_state['values']['email'] : "");

  	$fid = $form_state['values']['fid'];
  	$type = $form_state['values']['type'];
  	$nseqs = $form_state['values']['nseqs'];
  	$totlen = $form_state['values']['totlen'];

	$type = substr($type,0,1); # n or p

	$fobj = file_load($fid);
	$fpath = drupal_realpath($fobj->uri);
	$ul_origname = drupal_basename($fpath);
	$cleaned_path = "$fpath.cleaned"; # The cleaned copy which should already be there

	# First we need a semi-private 'jobkey' for this job so only the
	# user in question can see their tracking page

	$jobkey = make_jobkey();
	while (jobkey_exists($jobkey))
	{
		$jobkey  = make_jobkey();
	}

	$start_time = time();
	$jid = db_insert('tripal_annot')->fields(array(
  		'jobkey' => $jobkey,
		'ultype' => $type,
		'ulbases' => $totlen,
		'ul_origname' => $ul_origname,
		'ulseqs' => $nseqs,
		'email' => $email,
		'start_time' => $start_time
	))->execute();

	#
	# Copy the cleaned user file into the tripal files location. 
	# This will be the query file for the jobs UNLESS it was a nucleotide file. 
	# In that case ESTScan output will be in a new file with ".trans" appended,
	# and that file will be used downstream. 
	#

	$upload_new_name = "upload_$jobkey";  # can't use the jid because then after uninstall/reinstall
	$upload_new_path = tripal_annot_build_file_path($upload_new_name);

	system("cp $cleaned_path $upload_new_path");
	system("chmod -w $upload_new_path"); # so can't accidentally delete during debugging
	tripal_annot_update_field($jid,"ulname",$upload_new_name);

	#
	# Queue the Tripal job that will do the work
	#

	$job_args = array(
		"jid" => $jid
	);

	$job_id = tripal_add_job(
	  "annot $jid",
	  "tripal_annot",
	  "run_tripal_annot",
	  $job_args,
	  $user->uid
	);
	tripal_annot_update_field($jid,"joblist",$job_id);
	
	tripal_annot_update_field($jid,"status","Your job is queued and will be started soon");

	if ($email != "")
	{
		$msg = <<< END
Your LIS annotation job has been started. Your job ID is $jobkey and the results will be available at 
http://www.legumeinfo.org/annot/job/$jobkey .
END;

		drupal_mail("tripal_annot", "mymail", $email, language_default(), 
			$params = array('msg' => $msg), "noreply@legumeinfo.org",  TRUE);
	}

	drupal_goto("annot/job/$jobkey" );

}
function run_tripal_annot($jid)
{
	#
	# INITIALIZING
	#
	# Build the job list and also status string lists to use reporting status
	# $joblist and $queued have same content but former is internal, latter is for status display
	# Jobs are popped off of $joblist and $queued in parallel, and added to $running and $done for status
	#
	$jinfo = tripal_annot_get_job_info($jid,"");
	$jobkey = $jinfo->jobkey;
	$joblist = array(); 				
	$queued = array(); $done = array();	
	$running = array(); $failed = array(); # these could be strings as long as no parallel jobs

	# ESTScan
	if ($jinfo->ultype == "n")
	{
		array_push($joblist, "escan");	
		array_push($queued, "ESTScan");	
	}
	# BLAST
	for ($j = 1; $j <= 4; $j++) # TODO: blasts hardcoded to 4 max
	{
		$dbpath = variable_get("annot_db$j","");
		if ($dbpath != "")
		{
			if (!file_exists("$dbpath"))
			{
				drupal_set_message("Can't find $dbpath!");
				return 0;
			}
			array_push($joblist, "blast:$j");
			array_push($queued, "BLAST ".basename($dbpath));
		}
	}
	# AHRD
	array_push($joblist, "ahrd");	
	array_push($queued, "AHRD");	
	# Interpro
	array_push($joblist, "ipr");	
	array_push($queued, "InterProScan");	
	# HMM
	array_push($joblist, "hmm");	
	array_push($queued, "HMMer");	
	# post
	array_push($joblist, "post");	
	array_push($queued, "Postprocessing");	

	#
	# RUNNING
	#

	while (count($joblist) > 0)
	{
		$nextjob = array_shift($joblist);
		$nextstat = array_shift($queued);	
		array_push($running,$nextstat);

		# Check if it was cancelled since Tripal will not actually terminate in-progress job
		$jinfo = tripal_annot_get_job_info($jid,"");
		if ($jinfo->status == "Cancelled")
		{
			return 0;
		}
		tripal_annot_update_job_status($jid,$queued,$running,$failed,$done,"");
		$error = tripal_annot_run_job($nextjob,$jid);

		if ($error != "")
		{
			array_push($failed,array_shift($running));		
			tripal_annot_log_error("ERROR in $nextjob for $jobkey\n$error\n###########\n");
			drupal_set_message("The annotation did not complete successfully!");
			tripal_annot_update_job_status($jid,$queued,$running,$failed,$done,$error);
			tripal_annot_update_field($jid,"error",$error);
			return 0;
		}
		else
		{
			array_push($done,array_shift($running));
		}
	}
	
	$end_time = time();
	db_update('tripal_annot')
		->fields(array(
	        "end_time" => $end_time 
	        )
		)
		->condition('jid', $jid)
		->execute();
	
	return 1;
}
#
# Prepare the user-facing status display, which is a list of jobs and their status,
# plus possible error
# Currently displayed in html <pre> block; else, will need html formatting included
#
function tripal_annot_update_job_status($jid,&$queued,&$running,&$failed,&$done,$error)
{
	$status = "";
	if (count($done) > 0) 
	{
		$status .= implode(" DONE\n",$done)." DONE\n"; 
	}
	if (count($failed) > 0)
	{
		$status .= implode(" FAILED\n",$failed)." FAILED\n"; # overkill since should be max one failure
	}
	if (count($running) > 0)
	{
		$status .= implode(" RUNNING\n",$running)." RUNNING\n"; # overkill since should be max one running 
	}
	if (count($queued) > 0)
	{
		$status .= implode(" QUEUED\n",$queued)." QUEUED\n"; 
	}
	if ($error != "")
	{
		#$status .= "Error Message:$error\n";
	}
	tripal_annot_update_field($jid,"status",$status);
}
function tripal_annot_run_job($name,$jid)
{
	$jinfo = tripal_annot_get_job_info($jid,""); # have to get it again to get the names of prior outputs
	if (0 == strncmp($name,"escan",3))
	{
		return tripal_annot_run_escan($jid,$jinfo);
	}
	else if (0 == strncmp($name,"blast",3))
	{
		$num = str_replace("blast:","",$name); # was passed as blast:DBnum
		return tripal_annot_run_blast($jid,$num,$jinfo);
	}
	else if (0 == strncmp($name,"ahrd",3))
	{
		return tripal_annot_run_ahrd($jid,$jinfo);
	}
	else if (0 == strncmp($name,"ipr",3))
	{
		return tripal_annot_run_ipr($jid,$jinfo);
	}
	else if (0 == strncmp($name,"hmm",3))
	{
		return tripal_annot_run_hmm($jid,$jinfo);
	}
	else if (0 == strncmp($name,"post",3))
	{
		return tripal_annot_run_post($jid,$jinfo);
	}
	die ("BAD JOB NAME");
}
function jobkey_exists($key)
{
	$r = db_query("select count(*) as cnt from tripal_annot where jobkey='$key'");
	return ($r->fetchObject()->cnt > 0);
}
function make_jobkey()
{
	$char1 = chr(ord('A') + rand(0,25));	
	$char2 = chr(ord('1') + rand(0,8));	
	$char3 = chr(ord('A') + rand(0,25));	
	$char4 = chr(ord('1') + rand(0,8));	
	$char5 = chr(ord('A') + rand(0,25));	
	return $char1.$char2.$char3.$char4.$char5;
}
function tripal_annot_run_blast($jid, $num, $jinfo)
{
	$jobkey = $jinfo->jobkey;
	$dbpath = variable_get("annot_db$num","");
	if ($dbpath == "")
	{
		return "BLAST database #$num was not specified!";
	}
	if (!file_exists("$dbpath"))
	{
		return "BLAST database #$num not found!";
	}


	$query = tripal_annot_build_query_path($jinfo);

	$blastpath = variable_get("annot_blast_path","");
	$blastprog = "blastp";
	if (!file_exists("$blastpath/$blastprog"))
	{
		return "Can't find $blastprog!";
	}
	$blastpath .= "/$blastprog";	
	
	$outpath = tripal_annot_build_blast_output_path($jobkey,$num);

	$threads = variable_get("annot_nthread",1);
	$params = " -db $dbpath -query $query -out $outpath -outfmt 6 -num_threads $threads ";
	$cmd = "$blastpath $params";

	system($cmd);

	if (!file_exists($outpath)) 
	{
		tripal_annot_log_error("BLAST FAILURE\n$cmd\n");
		return "BLAST failed to DB#$num";
	}
	return "";
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
function tripal_annot_run_escan($jid,$jinfo)
{
	$jobkey = $jinfo->jobkey;
	$escanpath = variable_get("annot_escanpath");
	if (!file_exists($escanpath))
	{
		return "Could not find estscan binary $escanpath!";
	}
	$smatpath = variable_get("annot_smat");
	if (!file_exists($smatpath))
	{
		return "Could not find smat file $smatpath!";
	}
	$params = variable_get("annot_escanparam");
	$upload_fpath = tripal_annot_build_file_path($jinfo->ulname);
	$outpath = "$upload_fpath.trans";

	##$cmd = "$escanpath -M $smatpath -o /tmp/escan.out -t $outpath $upload_fpath";
	##file_put_contents("/tmp/escan.txt","$cmd\n",FILE_APPEND);
	$cmd = "$escanpath -M $smatpath -o /dev/null -t $outpath $upload_fpath";
	system($cmd);

	if (!file_exists($outpath) || 0==filesize($outpath))
	{
		return "ESTScan failed! (no output file, or empty)";
	}
	return "";
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
#
# this function builds a path to the final query file, after
# cleaning, renaming, and possible translation through estscan
#
function tripal_annot_build_query_path($jinfo)
{
	$name = $jinfo->ulname;
	if ($jinfo->ultype == "n") $name .= ".trans";
	return tripal_annot_build_file_path($name);
}
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
