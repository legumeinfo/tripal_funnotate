<?php

#####################################################################################
#
# Upload file page: Landing page after upload
#					Shows sanity check info about the uploaded file 
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
	$curName = "";
	$curSeq = "";
	$numSeqs = 0;
	$totalLen = 0;
	$seqType = ""; # n or p
	# every seq name will go into exactly one of the below arrays
	$zeroLenSeqs = array();
	$ambigSeqs = array();
	$protSeqs = array();
	$ntSeqs = array();
	while (($line = fgets($fh)) !== false)
	{
		if (preg_match('/^>(\S+)/',$line,$matches))
		{
			if ($curName != "")
			{
				$numSeqs++;
				if (strlen($curSeq) > 0)
				{
					$curType = get_seq_type($curSeq, $curName);
					if ($curType == 'x')
					{
						array_push($ambigSeqs,$curName);
					}
					else if ($curType == "n")
					{
						array_push($ntSeqs,$curName);
					}
					else if ($curType == "p")
					{
						array_push($protSeqs,$curName);
					}
					$totalLen += strlen($curSeq);
				}
				else
				{
					array_push($zeroLenSeqs,$curName);		
				}
			}
			$curName = $matches[1];
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
			$curType = get_seq_type($curSeq, $curName);
			if ($curType == 'x')
			{
				array_push($ambigSeqs,$curName);
			}
			else if ($curType == "n")
			{
				array_push($ntSeqs,$curName);
			}
			else if ($curType == "p")
			{
				array_push($protSeqs,$curName);
			}
			$totalLen += strlen($curSeq);
		}
		else
		{
			array_push($zeroLenSeqs,$curName);		
		}
	}

	fclose($fh);

	$numProt = count($protSeqs);
	$numNT = count($ntSeqs);
	if ($numProt == $numSeqs)
	{
		$seqType = "protein";
	}
	else if ($numNT == $numSeqs)
	{
		$seqType = "nucleotide";
	}
	
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
	else if ($seqType == "")
	{
		$numambig = count($ambigSeqs);
		$numzero = count($zeroLenSeqs);
		
		if ($numzero > 0)
		{
			$text .= "<p>$numzero zero-length sequences were found.<p>";
		}
		if ($numambig > 0)
		{
			$text .= "<p>$numambig sequences could not be identified as nucleotide or protein.<p>";
		}
		if ($numProt >0 && $numNT > 0)
		{
			$text .= "<p>The file contains both nucleotide and protein sequences ($numProt protein, ";
			$text .= "$numNT nucleotide).<p>";
		}
		$text .= <<<END
<p>
Please ensure that the file contains either all nucleotide or all protein sequences<br>
and that no sequences are empty.
<p>
Also the file should be a plain-text file, i.e., not a Word
<br>
document, rich-text file, or other file containing formatting characters. 
<p>
The file must be in the FASTA format,
 using only the standard IUPAC codes.
<br>
The allowed codes are (upper/lowercase doesn't matter):
<br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;nucleotides:  agctnkmbvswdyrh-
<br>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;protein: abcdefghiklmnpqrstuvwxyz*-
END;
		if ($numzero > 0)
		{
			$text .= "<hr><br>Zero-length sequences:<br>";
			$text .= implode("<br>",$zeroLenSeqs);
		}
		if ($numambig > 0)
		{
			$text .= "<hr><br>Ambiguous sequences:<br>";
			$text .= implode("<br>",$ambigSeqs);
		}
		if ($numProt > 0 && $numNT > 0)
		{
			$text .= "<hr><br>Nucleotide sequences:<br>";
			$text .= implode("<br>",$ntSeqs);
			$text .= "<hr><br>Protein sequences:<br>";
			$text .= implode("<br>",$protSeqs);
		}
		if ($ta_debug)
		{
			$text .= "<hr><p>";
			$text .= $debug_text;
		}
		return $text;
	}
	else
	{
		$text .= <<<END
Sequence type: $seqType<br>
Number of sequences: $numSeqs <br>
Total sequence length: $totalLen<br>
<p>
<b>Please verify that this is correct before starting your job.</b>
<p>
END;
		$text .= drupal_render(drupal_get_form('annot_upload_form', 
									$fid, $seqType, $numSeqs, $totalLen)); 
		
	}
	return $text;
}
# Figure out protein or nucleotide by deleting the the iupac-allowed chars
# and seeing if anything is left. 
# If result is ambiguous, save it off for a debug message. 
function get_seq_type($seq, $seqName)
{
	global $ta_debug;
	global $debug_text;
	$total = strlen($seq);
	$ntrep = preg_replace('/[agctnkmbvswdyrhx\-]/','',$seq);
	$ntreplen = strlen($ntrep);
	if ($ntreplen == 0)
	{
		return 'n';
	}
	$aarep = preg_replace('/[abcdefghiklmnpqrstuvwxyz*\-\.]/','',$seq);
	$aareplen = strlen($aarep);
	if ($aareplen == 0)
	{
		return 'p';
	}
	if ($ta_debug)
	{
		$debug_text .= "$seqName: ntrep: $ntrep<br>";
		$debug_text .= "$seqName: aarep: $aarep<br>";
	}
	return 'x'; 
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

	# First we need a semi-private tag for this job so only the
	# user in question can see their tracking page

	$jobkey = make_jobkey();
	while (jobkey_exists($jobkey))
	{
		$jobkey  = make_jobkey();
	}

	$jid = db_insert('tripal_annot')->fields(array(
  		'jobkey' => $jobkey,
		'ulfid' => $fid,
		'ultype' => $type,
		'ulbases' => $totlen,
		'ulseqs' => $nseqs,
		'email' => $email,
	))->execute();

	# Now start the blast jobs (up to 4)
	$joblist = array(); # list of entries "jobid:dbnumber" to be stored in DB for use by tracking page
	$dblist = array(); # db numbers used, needed by the ahrd job
	for ($j = 1; $j <= 4; $j++)
	{
		$jobid = start_blast_job($jid,$j,$fpath,$type);
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
	$char2 = chr(ord('A') + rand(0,25));	
	$char3 = chr(ord('A') + rand(0,25));	
	$char4 = chr(ord('A') + rand(0,25));	
	return $char1.$char2.$char3.$char4;
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
	$outpath = DRUPAL_ROOT."/".tripal_get_files_dir("annot")."/$outname";
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
	$status .= "<p>$newstatus";
	tripal_annot_update_field($jid,"status",$status);
	
}
