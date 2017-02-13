<?php

#####################################################################################
#
# Job tracking page: Track job status and provide results for download
#
#######################################################################################

function annot_job_page($jobkey)
{
	if (isset($_GET["family"] ))
	{
		$fID = $_GET["family"];
		if (!preg_match('/^[0-9]+$/',$fID))
		{
			return "<p>Invalid gene family!<p>";
		}
		return tripal_annot_family_tree($jobkey,$fID);
	}
	$text = "";
	$refresh_text = <<<END
 
<script>
var time = new Date().getTime();
function refresh() 
{
	 if(new Date().getTime() - time >= 20000) 
		 window.location.reload(true);
	 else 
		 setTimeout(refresh, 20000);
}

setTimeout(refresh, 20000);
</script>
END;

	$result = db_query("select * from tripal_annot where jobkey='$jobkey'");
	if ($result->rowCount() == 0)
	{
		$text = "<p>Job <tt>$jobkey</tt> was not found.<p>";
		return $text;
	}

	$jarr = $result->fetchAssoc();
	$jid = $jarr["jid"];
	$email = $jarr["email"];
	$ul_origname = $jarr["ul_origname"];
	$ulname = $jarr["ulname"];
	$ahrdname = $jarr["ahrdname"];
	$sumname = $jarr["sumname"];
	$iprname = $jarr["iprname"];
	$hmmname = $jarr["hmmname"];
	$blastname = $jarr["blastname"];
	$nseqs = $jarr["ulseqs"];
	$totlen = $jarr["ulbases"];
	$seqtype = $jarr["ultype"];
	$status = $jarr["status"];   # this is a list that the job updates showing progress
	$errstr = $jarr["error"];   # this is the error message if one of the tasks failed
	$job_id = $jarr["joblist"];
	$start_time = $jarr["start_time"];
	$end_time = $jarr["end_time"];

	$basetype = ($seqtype == "n" ? "basepairs" : "proteins");
	$seqtypestr = ($seqtype == "n" ? "nucleotide" : "protein");

	$text .= <<<END
<h3>Annotation Job $jobkey</h3>
Uploaded file: $ul_origname ($seqtypestr, $nseqs sequences, $totlen total $basetype)

END;

	#
	# See if the tripal job is waiting, running, cancelled, done (with or without error)
	#
	if ($status == "Cancelled")  
	{
		# Note that this status is placed by our code rather than waiting for
		# Tripal to cancel the job. So the job may actually still be running. 
		
		$text .= "<p><p>The annotation was cancelled.<p>";
		return $text;
	}

	# Now get status information from Tripal jobs itself
	$result = db_query("select * from tripal_jobs where job_id=$job_id"); 
	if (!$result)
	{
		$text .= "<p>Error - the annotation job was not found! (<tt>id:$job_id</tt>)";
		return $text;
	}
	$tj_info = $result->fetchObject();			

	if ($tj_info->start_time == NULL)
	{
		$text .= "<p><p>The annotation job is queued and will begin shortly.<br>This page will automatically refresh.<p>";
		$text .= $refresh_text;
		return $text;
	}
	else if ($tj_info->status != "Completed" || $errstr != "")
	{
		#
		# It's either in progress, or it erred out.
		#
		# In either case, we show the last status string so we can what task it is (was) on. 
		#
		$text .= <<<END
<pre>
Job status: 
$status
</pre>

END;
		if ($tj_info->status == "Error" || $errstr != "")
		{
			#
			# An error was reported, show alarming message and end
			#
			if ($errstr == "") $errstr = "no text available";
			$text .= <<< END
<p>
<span style="color:red"><b>Job did not complete successfully!</b></span><br>
Error text: $errstr
END;
		}
		else
		{
			#
			# It's still going, no error, so show the cancel button, and set to autorefresh
			#
			$run_secs = time() - $start_time;
			$run_mins = floor($run_secs/60);
			$run_secs -= 60*$run_mins;
			$text .= $refresh_text;
			$text .= "<span style='font-size:11px'>job duration:$run_mins minutes, $run_secs seconds</span><br>"; 
			$text .= drupal_render(drupal_get_form('annot_cancel_form', $jid, $jobkey));
		}
		return $text;
	}

	#
	# The job is finished with apparent success. Notify and show final results. 
	#

	if ($email != "")
	{
		$msg = <<< END
Your LIS annotation job is complete. You can view the results at  
http://www.legumeinfo.org/annot/job/$jobkey .
END;

		drupal_mail("tripal_annot", "mymail", $email, language_default(), 
			$params = array('msg' => $msg), "noreply@legumeinfo.org",  TRUE);
	}

	$url_ahrd = tripal_annot_file_url($ahrdname);
	$url_ipr = tripal_annot_file_url($iprname);
	$url_hmm = tripal_annot_file_url($hmmname);
	$url_sum = tripal_annot_file_url($sumname);
	$url_escan = tripal_annot_file_url("$ulname.trans");

	$blast_file_links = build_blast_file_links($jobkey);

	$stats = array();
	$err = annot_calc_stats($stats, $jarr, $sumname);
	if ($err != "")
	{
		$text .= $err;
#		return $text;
	}
	$hits = $stats["hits"]; $pcthit = $stats["pcthit"];
	$gohits = $stats["go"]; $pctgo = $stats["pctgo"];

	$text .= <<<END
<table style="width:400px">
	<tr>
		<td>Number of annotated sequences:</td>
		<td>$hits ($pcthit%)</td>
	</tr>
	<tr>
		<td>Number with GO assignment:</td>
		<td>$gohits ($pctgo%)</td>
	</tr>
</table>
END;
if ($seqtype == "n")
{
	$text .= " <p> <a href='$url_escan' style='text-decoration:underline'>ESTScan output</a>\n";
}

	$text .= <<<END
<p>
BLAST output files: $blast_file_links
<br>
<a href="$url_ipr" style="text-decoration:underline">InterPro output</a> (txt file, RAW format)
<br>
<a href="$url_hmm" style="text-decoration:underline">HMM output</a> (txt file,tabular)
<br>
<a href="$url_ahrd" style="text-decoration:underline">AHRD output</a> (txt file,tabular)
<p>
Summary Table 
(right-click <a href="$url_sum" style="text-decoration:underline">download</a>) :
<table>
END;

$fpath = tripal_annot_build_file_path($sumname);
$fh = fopen($fpath,"r");
$linenum = 0;
while ( ($line = fgets($fh)) != FALSE)
{
	$linenum++;
	$fields = preg_split("/\t/",$line);
	if ($linenum == 1)
	{
		$fields[2] = "AHRD&nbsp;Quality<br><a href='#quality' style='text-decoration:underline'>explain</a>";
	}
	if ($linenum > 1)
	{
		if (trim($fields[4]) != "")
		{
			$iprs = explode(",",$fields[4]);
			$linkouts = array();
			foreach ($iprs as $i => $ipr)
			{
				$linkout = "<a href='https://www.ebi.ac.uk/interpro/entry/$ipr'>$ipr</a>";
				if ($i % 4 == 3)
				{
					$linkout .= "<br>";
				}
				array_push($linkouts,$linkout);
			}
			$fields[4] = implode("&nbsp;",$linkouts);
		}
		if (trim($fields[5]) != "")
		{
			$gos = explode(",",$fields[5]);
			$linkouts = array();
			foreach ($gos as $i => $go)
			{
				$linkout = "<a href='http://amigo.geneontology.org/amigo/term/GO:$go'>$go</a>";
				if ($i % 4 == 3)
				{
					$linkout .= "<br>";
				}
				array_push($linkouts,$linkout);
			}
			$fields[5] = implode("&nbsp;",$linkouts);
		}
		if (trim($fields[6]) != "") # only one gene family so far but....
		{
			$gfs = explode(",",$fields[6]);
			$linkouts = array();
			foreach ($gfs as $i => $gf)
			{
				#$linkout = "<a href=\"/chado_phylotree/phytozome_10_2.$gf\">$gf</a>";
				$linkout = "<a href=\"/annot/job/$jobkey?family=$gf\">$gf</a>";
				if ($i % 4 == 3)
				{
					$linkout .= "<br>";
				}
				array_push($linkouts,$linkout);
			}
			$fields[6] = implode("&nbsp;",$linkouts);
		}
	}
	$row = "<tr><td>".implode("</td><td>",$fields)."</td></tr>\n";
	$text .= $row;
}
$text .= "\n</table>\n";

$run_secs = $end_time - $start_time;
$run_mins = floor($run_secs/60);
$run_secs -= 60*$run_mins;
$text .= "<span style='font-size:11px'>job duration:$run_mins minutes, $run_secs seconds</span><br>"; 

$text .= <<<END
<a name="quality"></a>
<b>AHRD Quality Scores:</b>
<p>
<pre>
The AHRD quality-code consists of a three character string, where each 
character is either "*" if the respective criteria is met or "_" otherwise. 
The meaning by position is as follows:

1	Bit score of the blast result is over 50 and e-value is better than e-10
2	Overlap of the blast result is at least 60%
3	Top token score of assigned descriptor is at least 0.5
</pre>
For further explanation of these codes and the AHRD algorithm, see
<a href="https://github.com/groupschoof/AHRD/blob/master/README.textile">AHRD Documentation</a>.
END;

	return $text;
}
#	Calculate simple statistics:
#	Number and % of sequences with hits , and with GO.
#	It would be nice to calc the num with hits to each
#	individual database however that requires scanning each
#	DB to build a table, moreover must use the same regex
#	as in the yml file. 

function annot_calc_stats(&$stats,$jarr,$sumname)
{
	$ret = "";
	$numSeqs = $jarr["ulseqs"];		
	$fpath = tripal_annot_build_file_path($sumname);
	$fh = fopen($fpath,"r");
	$numhit = 0;
	$numgo = 0;
	$line = fgets($fh); # header
	while ( ($line = fgets($fh)) != FALSE)
	{
		if (!preg_match('/\S/',$line))
		{
			break; # empty line, end of file presumably
		}
		$f = preg_split('/\t/',$line);
		if ( count($f) != 7) 
		{
			$nf = count($f);
			$ret = "<p><br><span style='color:red'>The Summary file does not have the right format!</span><br>Bad line:<br>$line<br>Num fields:$nf<p>";
			return $ret;
		}
		if (trim($f[3]) != "" && trim($f[3]) != "Unknown protein")
		{
			$numhit++;
		}
		if (trim($f[4]) != "")
		{
			$numgo++;
		}	
	}
	fclose($fh);

	$stats["hits"] = $numhit;
	$stats["go"] = $numgo;
	$stats["pcthit"] = floor((100*$numhit)/$numSeqs);
	$stats["pctgo"] = floor((100*$numgo)/$numSeqs);
	return $ret;
}
#TODO is this robust?
function tripal_annot_file_url($fname)
{
	$subdir = variable_get("annot_files_dir");
	return file_create_url("public://tripal/$subdir/$fname");
}
function annot_cancel_form($form,&$form_state)
{
	$jid = $form_state['build_info']['args'][0];
	$jobkey = $form_state['build_info']['args'][1];

    $form = array();

	$form['jid'] = array(
		'#type' => 'hidden',
		'#value' => $jid,
	);
	$form['jobkey'] = array(
		'#type' => 'hidden',
		'#value' => $jobkey,
	);
	$form['submit'] = array(
		'#type' => 'submit',
		'#value' => 'Cancel Annotation',
	  );

	return $form;
}
function annot_cancel_form_submit($form, &$form_state)
{
	global $user;
  	$jid = $form_state['values']['jid'];
  	$jobkey = $form_state['values']['jobkey'];

	$result = db_query("select status, joblist from tripal_annot where jid='$jid'");
	if ($result->rowCount() == 0)
	{
		$text = "<p>Job <tt>$jid</tt> was not found.<p>";
		return $text;
	}
	$jarr = $result->fetchAssoc();
	$status = $jarr["status"];
	$jobid = $jarr["joblist"];

	tripal_cancel_job($jobid,FALSE);	
	tripal_annot_update_field($jid,"status","Cancelled");

	drupal_goto("/annot/job/$jobkey");
}
function build_blast_file_links($jobkey)
{
	$links = array();
	for ($j = 1; $j <= 4; $j++) # TODO: blasts hardcoded to 4 max
	{
		$dbpath = variable_get("annot_db$j","");
		if ($dbpath != "")
		{
			$text = basename($dbpath);
			$name = "blast_$jobkey"."_$j";
			$url = tripal_annot_file_url($name);
			$link = "<a href='$url' style='text-decoration:underline'>$text</a>";
			array_push($links,$link);
		}
	}
	return implode(", ",$links);
}
function tripal_annot_family_tree($jobkey,$fID)
{
	$fID = trim($fID); # just in case
	$text = "";
	$jinfo = tripal_annot_get_job_info(0,$jobkey);
	if (!$jinfo)
	{
		$text = "<p>Job <tt>$jobkey</tt> was not found.<p>";
		return $text;
	}
	$ulpath = tripal_annot_build_file_path($jinfo->ulname); # uploaded sequence file, cleaned
	if ($jinfo->ultype == "n") $ulpath .= ".trans";	
	if (!file_exists($ulpath))
	{
		$text = "<p>Unable to find uploaded sequence file!<p>";
		tripal_annot_log_error("Can't find upload file $ulpath");
		return $text;
	}
	$sumpath = tripal_annot_build_file_path($jinfo->sumname); # uploaded sequence file, cleaned
	if (!file_exists($sumpath))
	{
		$text = "<p>Unable to find summary table file!<p>";
		tripal_annot_log_error("Can't find summary file $ulpath");
		return $text;
	}
	#
	#	First: collect the user sequence names assigned to this gene family
	#
	$fh = fopen($sumpath,"r");
	$line = fgets($fh); # header
	$user_seqs = array();
	while ( ($line = fgets($fh)) != FALSE)
	{
		$fields = preg_split("/\t/",$line);
		if (trim($fields[6]) != "") # only one gene family so far but....
		{
			$gfs = explode(",",$fields[6]);
			$match = 0;
			foreach ($gfs as $i => $gf)
			{
				$gf = trim($gf); # it's the end of the line
				if ($gf == $fID)
				{
					$match = 1;
					break;
				}
			}
			if ($match)
			{
				$seqname = trim($fields[0]);
				$user_seqs[$seqname] = "";
			}
		}	
	}
	fclose($fh);

	# 
	# Now collect the sequences for these names
	#
	$fh = fopen($ulpath,"r");
	$curName = "";
	while ( ($line = fgets($fh)) != FALSE)
	{
		if (preg_match('/^>(\S+)(\s.*)/',$line,$matches))
		{
			$curName = $matches[1];
		}	
		else
		{
			if (isset($user_seqs[$curName]))
			{
				$user_seqs[$curName] .= trim($line);
			}
		}
	}
	#
	#	Now get the gene family sequences
	#
	$gfdir = variable_get("annot_hmmfamdir","");
	if (!is_dir($gfdir))
	{
		log_error("Gene family directory $gfdir not found");
		$text .= "<p>Configuration error: unable to find the gene family fasta sequences<p>";
		return $text;
	}
	$gffile = "$gfdir/$fID";
	if (!file_exists($gffile))
	{
		log_error("Gene family file $gffile not found");
		$text .= "<p>Configuration error: unable to find the gene family fasta sequences for $fID<p>";
		return $text;
	}
	$seqs = file_get_contents($gffile);
	if (!$seqs)
	{
		log_error("Gene family file $gffile unreadable or empty");
		$text .= "<p>Error: gene family sequences for $fID could not be loaded<p>";
		return $text;
	}
	#
	# Write all the seqs to a temp file for upload to Lorax
	#
	$tmpdir = file_directory_temp();	
	$gf_attach = "$tmpdir/gf_$jobkey.fa";
	file_put_contents($gf_attach,$seqs);
	foreach ($user_seqs as $name => $seq)
	{
		$entry = ">$name\n";
		for ($i = 0; $i < strlen($seq); $i += 50)
		{
			$entry .= substr($seq,$i,50);
			$entry .= "\n";
		}
		file_put_contents($gf_attach,$entry,FILE_APPEND);
	}
	return $text;
}
