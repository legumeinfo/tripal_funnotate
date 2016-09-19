<?php

#####################################################################################
#
# Job tracking page: Track job status and provide results for download
#
#######################################################################################

function annot_job_page($jobkey)
{
	$text = "";
	$result = db_query("select * from tripal_annot where jobkey='$jobkey'");
	if (!$result)
	{
		$text = "<p>Job <tt>$jobkey</tt> was not found.<p>";
		return $text;
	}

	$jarr = $result->fetchAssoc();
	$error = $jarr["status"];
	$email = $jarr["email"];
	$fid = $jarr["ulfid"];
	$outfid = $jarr["outfid"];
	$blastfid = $jarr["blastfid"];
	$nseqs = $jarr["ulseqs"];
	$totlen = $jarr["ulbases"];
	$seqtype = ($jarr["ultype"] == "n" ? "nucleotide" : "protein");
	$basetype = ($jarr["ultype"] == "n" ? "basepairs" : "proteins");

	$joblist = explode(",",$jarr["joblist"]);
	$ahrd_jobid = array_pop($joblist);
	if (!is_numeric($ahrd_jobid))
	{
		$text = "Unable to determine AHRD job!<p>Error occurred.<p>";
		return $text;
	}
	$fobj = file_load($fid);
	$fname = $fobj->filename;

	$text = <<<END
<h3>Annotation Job $jobkey</h3>
Uploaded file: $fname ($seqtype, $nseqs sequences, $totlen total $basetype)

END;

	if ($error != "")
	{
		$text .= <<< END
<p><span style="color:red"><b>$error</b></span><p>
END;
	}

	$status_str = ""; 
	$numleft = 0;
	foreach ($joblist as $jobstr)
	{
		$jnarr = explode(":",$jobstr);
		$jobid = $jnarr[0];
		$dbnum = $jnarr[1];
		$dbname = basename(variable_get("annot_db$dbnum"));
		$result = db_query("select * from tripal_jobs where job_id=$jobid"); 
		if (!$result)
		{
			$text .= "<p>Job <tt>$jobid</tt> (BLAST $dbname) was not found!<p>An error occurred.";
			return $text;
		}
		$r = $result->fetchObject();			
		if (NULL == $r->start_time)
		{
			$status .= "$dbname BLAST queued\n";
			$numleft++;
		}
		else
		{
			if (NULL == $r->end_time)
			{
				$status .= "$dbname BLAST running\n";
				$numleft++;
			}
			else
			{
				$status .= "$dbname BLAST done\n";
			}
		}
	}
	$result = db_query("select * from tripal_jobs where job_id=$ahrd_jobid"); 
	if (!$result)
	{
		$text .= "<p>Job <tt>$ahrd_jobid</tt> (AHRD) was not found!<p>An error occurred.";
		return $text;
	}
	$r = $result->fetchObject();			
	if (NULL == $r->start_time)
	{
		$status .= "AHRD queued\n";
		$numleft++;
	}
	else
	{
		if (NULL == $r->end_time)
		{
			$status .= "AHRD running\n";
			$numleft++;
		}
		else
		{
			$status .= "AHRD done\n";
		}
	}

	#
	# Jobs are still running so just show the status of the jobs
	#
	if ($numleft > 0) 
	{
		$text .= <<<END
<pre>
Job in progress: 
$status
</pre>

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

	$fobj = file_load($outfid); # the AHRD output
	$outurl = file_create_url($fobj->uri);

	$fobj2 = file_load($blastfid); #  the blast gzip
	$outurl2 = file_create_url($fobj2->uri);

	$stats = array();
	$err = annot_calc_stats($stats, $jarr, $fobj);
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
<p>
<a href="$outurl" style="text-decoration:underline">AHRD Annotation Output</a>
 (right-click to download)
<br>
<a href="$outurl2" style="text-decoration:underline">BLAST output files</a> (tar/gzip archive)
<p>
Or view the output here:<br>
<table>
END;

$fpath = drupal_realpath($fobj->uri);
$fh = fopen($fpath,"r");
$startread = 0;
while ( ($line = fgets($fh)) != FALSE)
{
	if (substr($line,0,17 ) == "Protein-Accession")
	{
		$startread = 1;
	}
	if ($startread)
	{
		$fields = preg_split("/\t/",$line);
		$row = "<tr><td>".implode("</td><td>",$fields)."</td></tr>\n";
		$text .= $row;
	}
}
$text .= "\n</table>\n";

	return $text;
}
#	Calculate simple statistics:
#	Number and % of sequences with hits , and with GO.
#	It would be nice to calc the num with hits to each
#	individual database however that requires scanning each
#	DB to build a table, moreover must use the same regex
#	as in the yml file. 

function annot_calc_stats(&$stats,$jarr,$fobj)
{
	$ret = "";
	$numSeqs = $jarr["ulseqs"];		
	$fpath = drupal_realpath($fobj->uri);
	$fh = fopen($fpath,"r");
	$numhit = 0;
	$numgo = 0;
	# read to the header line - or could skip 3 lines
	while ( ($line = fgets($fh)) != FALSE)
	{
		if (substr($line,0,17 ) == "Protein-Accession")
		{
			break;
		}
	}	
	while ( ($line = fgets($fh)) != FALSE)
	{
		if (!preg_match('/\S/',$line))
		{
			break; # empty line, end of file presumably
		}
		$f = preg_split('/\t/',$line);
		if (count($f) != 6)
		{
			$nf = count($f);
			$ret = "<p><br><span style='color:red'>The output AHRD file does not have the right format!</span><br>Bad line:<br>$line<br>Num fields:$nf<p>";
			return $ret;
		}
		if (trim($f[3]) != "" && trim($f[3]) != "Unknown protein")
		{
			$numhit++;
		}
		if (trim($f[5]) != "")
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