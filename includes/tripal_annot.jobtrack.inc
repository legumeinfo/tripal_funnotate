<?php

#####################################################################################
#
# Job tracking page: Track job status and provide results for download
#
#######################################################################################

function annot_job_page($jobkey)
{
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
				$linkout = "<a href=\"/chado_phylotree/phytozome_10_2.$gf\">$gf</a>";
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
