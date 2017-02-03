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
	$status = $jarr["status"];
	$job_id = $jarr["joblist"];

	$basetype = ($seqtype == "n" ? "basepairs" : "proteins");
	$seqtypestr = ($seqtype == "n" ? "nucleotide" : "protein");

	# See if the tripal job is waiting, running, done, or cancelled
	# STARTHERE
$result = db_query("select * from tripal_jobs where job_id=$escan_jobid"); 
		if (!$result)
		{
			$text .= "<p>Job <tt>$escan_jobid</tt> (ESTScan) was not found!<p>An error occurred.";
			return $text;
		}
		$r = $result->fetchObject();			
		$jobstat = $r->status;
		if ($jobstat == "Cancelled")
		{
			$status .= "ESTScan cancelled\n";
		}	

	$text .= <<<END
<h3>Annotation Job $jobkey</h3>
Uploaded file: $ul_origname ($seqtypestr, $nseqs sequences, $totlen total $basetype)

END;
	if ($status == "Cancelled")
	{
		$text .= "<p><p>Job was cancelled by user.<p>";
		return $text;
	}

		$text .= "<p>Job <tt>$post_jobid</tt> (postprocess) was not found!<p>An error occurred.";
		return $text;
	}
	$r = $result->fetchObject();			
	$jobstat = $r->status;
	if ($jobstat == "Cancelled")
	{
		$status .= "Postprocessing cancelled\n";
	}	
	else if (NULL == $r->start_time)
	{
		$status .= "Postprocessing queued\n";
		$numleft++;
	}
	else
	{
		if (NULL == $r->end_time)
		{
			$status .= "Postprocessing running\n";
			$numleft++;
		}
		else
		{
			$errmsg = $r->error_msg;
			if ($jobstat == "Error" || $errmsg != "")
			{
				$status .= "Postprocessing ERROR: $errmsg\n";
				$error = "Postprocessing";
			}	
			else
			{
					$status .= "Postprocessing done\n";
			}
		}
	}

	#
	# Jobs are still running so just show the status of the jobs
	#
	if ($numleft > 0) 
	{
		$text .= $refresh_text;
		$text .= <<<END
<pre>
Job in progress: 
$status
</pre>

END;
		$text .= drupal_render(drupal_get_form('annot_cancel_form', $jid, $jobkey));
		return $text;
	}
	if ($error != "")
	{
		$text .= <<< END
<pre>
Job status: 
$status
</pre>
<span style="color:red"><b>Job did not complete successfully!</b></span><br>
END;
		return $text;
	}
	#
	# All done! Notify and show final results. 
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
	$url_blast = tripal_annot_file_url($blastname);
	$url_ipr = tripal_annot_file_url($iprname);
	$url_hmm = tripal_annot_file_url($hmmname);
	$url_sum = tripal_annot_file_url($sumname);
	$url_escan = tripal_annot_file_url("$ulname.trans");

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
<a href="$url_blast" style="text-decoration:underline">BLAST output files</a> (tar/gzip archive)
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
For more about the AHRD algorithm, see
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
	$joblist = $jarr["joblist"];

	$jobs = array_reverse(explode(",",$joblist));
	foreach ($jobs as $jobstr)
	{
		$jobid = preg_replace("/:.*/","",$jobstr); # some of them are jobid:number
		tripal_cancel_job($jobid,FALSE);	
	}
	tripal_annot_update_field($jid,"status","Cancelled");

	drupal_goto("/annot/job/$jobkey");
}
