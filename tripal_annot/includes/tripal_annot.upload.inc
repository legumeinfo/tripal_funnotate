<?php

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
		if (preg_match('/^>(\S+)/',$line,$matches))
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
					write_cleaned_sequence($fhtemp,$curHeader,$cleanedSeq);
				}
				else
				{
					array_push($zeroLenSeqs,$curName);		
				}
			}
			$curName = $matches[1];
			$curHeader = $line;
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
			write_cleaned_sequence($fhtemp,$curHeader,$cleanedSeq);
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
The following sequences look like nucleotide sequences. Please verify that they <br>
are correct protein sequences:<br> $seqList <p>
END;
		}
		if ($seqType == "n" && count($wrongTypeSeqs) > 0)
		{
			$seqList = implode("<br>\n",$wrongTypeSeqs);
			$text .= <<<END
<p><b>Warning:</b><br>
The following sequences do not look like nucleotide sequences. Please verify that they <br>
are correct nucleotide sequences:<br> $seqList <p>
END;
		}
		$text .= <<<END
<p>
<b>Please verify that this is correct before starting your job.</b>
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
	if (strlen($test) < strlen($seq)/2)
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
function write_cleaned_sequence($fh,$header,$seq)
{
	fwrite($fh,$header);
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

    $form = array();

	$form['email'] = array(
		'#type' => 'textfield',
		'#title' => "Email (if you wish to be notified):",
		'#size' => 25
	);

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
  	$email = $form_state['values']['email'];
  	$fid = $form_state['values']['fid'];

	$fobj = file_load($fid);
	$fpath = drupal_realpath($fobj->uri);
	if (!file_exists($fpath))
	{
			form_set_error('fid', t('The uploaded file is no longer available! Please re-upload.'));
	}
}

#
# Steps:
# Add cleaned file to file_managed
# Create job key string
# Make DB entry
# Launch jobs
# Email notify
# Redirect to tracking page
#
function annot_upload_form_submit($form, &$form_state)
{
	global $user;

	#annot_update_schema();
  	$email = $form_state['values']['email'];
  	$fid = $form_state['values']['fid'];
  	$type = $form_state['values']['type'];
  	$nseqs = $form_state['values']['nseqs'];
  	$totlen = $form_state['values']['totlen'];

	$type = substr($type,0,1); # n or p

	$fobj = file_load($fid);
	$fpath = drupal_realpath($fobj->uri);
	$ul_origname = drupal_basename($fpath);
	$cleaned_path = "$fpath.cleaned"; # The cleaned copy that we will be actually using

	# First we need a semi-private tag for this job so only the
	# user in question can see their tracking page

	$jobkey = make_jobkey();
	while (jobkey_exists($jobkey))
	{
		$jobkey  = make_jobkey();
	}

	$jid = db_insert('tripal_annot')->fields(array(
  		'jobkey' => $jobkey,
		'ulfid' => 0,  # to be updated immediately
		'ultype' => $type,
		'ulbases' => $totlen,
		'ulname' => $ul_origname,
		'ulseqs' => $nseqs,
		'email' => $email,
	))->execute();

	$upload_new_name = "upload_$jobkey";  # can't use the jid because then after uninstall/reinstall
										# the jid resets to 1 but the earlier managed files 
										# are still in there. We could try to remove them
										# but this is more robust
	$upload_new_path = tripal_annot_build_file_path($upload_new_name);
	system("cp $cleaned_path $upload_new_path");
	$new_fid = annot_add_managed_file($upload_new_name);
	tripal_annot_update_field($jid,"ulfid",$new_fid);

	# Now start the blast jobs (up to 4)
	$joblist = array(); # list of entries "jobid:dbnumber" to be stored in DB for use by tracking page
	$dblist = array(); # db numbers used, needed by the ahrd job
	for ($j = 1; $j <= 4; $j++)
	{
		$jobid = start_blast_job($jid,$j,$upload_new_path,$type);
		if ($jobid != 0)
		{
			array_push($joblist, "$jobid:$j");
			array_push($dblist,$j);
		}
	}
	if (count($joblist) == 0)
	{
		drupal_set_message(t("No blast jobs were able to start!"));	
		tripal_annot_append_job_error($jid,t("Blast jobs were unable to start!"));
		drupal_goto("annot/job/$jobkey" );
	}

	# Now start the ahrd job
	$dbstr = join(",",$dblist);
	$job_args = array(
		"dblist" => $dbstr,
		"jid" => $jid
	);

	$job_id = tripal_add_job(
	  "annot $jid AHRD",
	  "tripal_annot",
	  "run_tripal_annot_ahrd",
	  $job_args,
	  $user->uid
	);
	array_push($joblist,$job_id);

	# Now start the interpro job
	$iprdir = variable_get("annot_iprdir","");
	if (!file_exists("$iprdir/interproscan.sh"))
	{
		drupal_set_message("Can't find interproscan.sh in $iprdir!");
		return 0;
	}
	$iprpath = "$iprdir/interproscan.sh";

	$tparam = ($type == "n" ? " -t n " : "");
	$iprparam = variable_get("annot_iprparam");
	
	$outname = "ipr_$jid.xml";
	$outpath = tripal_annot_build_file_path($outname);
	$params = "$tparam -i $upload_new_path -o $outpath -f XML $iprparam ";
	$cmd = "$iprpath $params";

	$job_args = array(
		"cmd" => $cmd,
		"out" => $outpath,
		"jid" => $jid);

	$job_id = tripal_add_job(
	  "annot $jid IPR",
	  "tripal_annot",
	  "run_tripal_annot_ipr",
	  $job_args,
	  $user->uid
	);
	array_push($joblist,$job_id);

	$jobstr = join(",",$joblist);
	db_update('tripal_annot')
		->fields(array(
	        'joblist' => $jobstr
	        )
		)
		->condition('jid', $jid)
		->execute();

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
function start_blast_job($jid, $num, $query, $type)
{
	global $user;
	annot_update_schema();
	$dbpath = variable_get("annot_db$num","");
	if ($dbpath == "")
	{
		return 0;
	}
	if (!file_exists("$dbpath"))
	{
		drupal_set_message("Can't find $dbpath!");
		return 0;
	}
	$blastpath = variable_get("annot_blast_path","");
	$blastprog = ($type == "n" ? "blastx" : "blastp");
	if (!file_exists("$blastpath/$blastprog"))
	{
		drupal_set_message("Can't find $blastprog in $blastpath!");
		return 0;
	}
	$blastpath .= "/$blastprog";	
	
	$outname = "blast_$jid"."_$num";
	$outpath = tripal_annot_build_file_path($outname);
	$params = " -db $dbpath -query $query -out $outpath -outfmt 6";
	$cmd = "$blastpath $params";

	$job_args = array(
		"cmd" => $cmd,
		"out" => $outpath,
		"jid" => $jid);

	$job_id = tripal_add_job(
	  "annot $jid blast $num",
	  "tripal_annot_$jid", 
	  "run_tripal_annot_blast",
	  $job_args,
	  $user->uid
	);
	return $job_id;
}
function run_tripal_annot_blast($cmd, $outfile, $jid)
{
	system($cmd);
	sleep(120);
	if (!file_exists($outfile)) 
	{
		tripal_annot_append_job_error( "Annot blast failed: $cmd");
		return FALSE;
	}
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
function tripal_annot_append_job_error($jid,$newstatus)
{
	$result = db_query("select status from tripal_annot where jid=$jid");
	$r = $result->fetchAssoc();
	$status = $r["status"];
	$status .= "$newstatus;";
	tripal_annot_update_field($jid,"status",$status);
	
}
function tripal_annot_file_dir()
{
	return DRUPAL_ROOT."/".tripal_get_files_dir("annot");
}
function tripal_annot_build_file_path($filename)
{
	return tripal_annot_file_dir()."/$filename";
}
