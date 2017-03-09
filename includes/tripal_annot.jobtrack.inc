<?php

error_reporting(E_ALL);
#####################################################################################
#
# Job tracking page: Track job status and provide results for download
#
#######################################################################################

function annot_job_page($jobkey)
{
	if (isset($_POST["json"]))
	{
		$text = "<pre>".$_POST["json"]."</pre>";
		return $text;
	}
	if (isset($_GET["family"] ))
	{
		$url = variable_get('annot_loraxurl','');
		if ($url == "")
		{
			return "<p>Unable to create gene family tree since no lorax server was configured<p>";
		}
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
function tripal_annot_family_tree_old($jobkey,$fID)
{
	$fID = trim($fID); # just in case
	$text = "";
	$jinfo = tripal_annot_get_job_info(0,$jobkey);
	if (!$jinfo)
	{
		$text = "<p>Job <tt>$jobkey</tt> was not found.<p>";
		return $text;
	}
	#
	# See if job is in progress
	#
	$lorax_url = variable_get('annot_loraxurl');
	$ch = curl_init();
	curl_setopt($ch, CURLOPT_URL, "$url/trees/$jobkey/FastTree/status");
	$ret = curl_exec($ch);
	

	$ulpath = tripal_annot_build_file_path($jinfo->ulname); # uploaded sequence file, cleaned
	if ($jinfo->ultype == "n") $ulpath .= ".trans";	
	if (!file_exists($ulpath))
	{
		$text = "<p>Unable to find uploaded sequence file!<p>";
		tripal_annot_log_error("Can't find upload file $ulpath");
		return $text;
	}
	$sumpath = tripal_annot_build_file_path($jinfo->sumname);
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
	#
	#	Get the HMM for this family
	#
	$hmmdbfile = variable_get("annot_hmmdb","");
	if (!is_file($hmmdbfile))
	{
		log_error("Gene family HMM database $hmmdbfile not found");
		$text .= "<p>Configuration error: unable to find the gene family definition file<p>";
		return $text;
	}
	$fh = fopen($hmmdbfile,"r");
	if (!$fh)
	{
		log_error("Gene family HMM database $hmmdbfile not readable");
		$text .= "<p>Configuration error: unable to read the gene family definition file<p>";
		return $text;
	}
	$curName = "";
	$hmmline = ""; 
	$hmmdata = "";
	while ( ($line = fgets($fh)) != FALSE)
	{
		if (preg_match('/^\s*HMMER.*/',$line,$matches))
		{
			if ($curName == $fID) break; # already found it
			$hmmline = $line;  # save it off because it comes before the name line
			$curName = "";
		}	
		else if (preg_match('/^\s*NAME\s+(\S+).*/',$line,$matches))
		{
			$curName = $matches[1];
			if ($curName == $fID)
			{
				$hmmdata = $hmmline;
			}
		}	
		if ($curName == $fID)
		{
			$hmmdata .= $line;
		}
	}
	fclose($fh);
	if ($hmmdata == "")
	{
		log_error("Unable to find HMM for $fID in database $hmmdbfile");
		$text .= "<p>Configuration error: unable to locate gene family definition<p>";
		return $text;
	}
	$gf_hmm = "$tmpdir/hmm_$jobkey.hmm";
	file_put_contents($gf_hmm,$hmmdata);
	#
	#	Finally make the lorax requests
	#
	#curl_setopt($ch, CURLOPT_URL, "$lorax_url/config");
	curl_setopt($ch, CURLOPT_POST,1);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER,1);
	$text .= "<p>$ret<p>";

	$url = "$lorax_url/trees/$jobkey/HMM";
	$text .= "<p>Try: $url<p>";
	curl_setopt($ch, CURLOPT_URL, $url);
	$cfile = new CURLFile($gf_hmm);
	$post = array("HMM" => $cfile);
	curl_setopt($ch, CURLOPT_POSTFIELDS, $post);
	$ret = curl_exec($ch);
	if ($ret == FALSE)
	{
		$text .= "HMM upload to lorax was not successful<p>";
		log_error("Failed to upload HMM $gf_hmm to lorax server $url");
		return $text;
	}
	$text .= "<p>$ret<p>";

	$url = "$lorax_url/trees/$jobkey/sequences";
	$text .= "<p>Try: $url<p>";
	curl_setopt($ch, CURLOPT_URL, $url);
	$cfile = new CURLFile($gf_attach);
	$post = array("peptide" => $cfile);
	curl_setopt($ch, CURLOPT_POSTFIELDS, $post);
	$ret = curl_exec($ch);
	if ($ret == FALSE)
	{
		$text .= "Sequence upload to lorax was not successful<p>";
		log_error("Failed to upload sequences $gf_attach to lorax server $url");
		return $text;
	}
	$text .= "<p>$ret<p>";

	$ch = curl_init();

	$url = "$lorax_url/trees/$jobkey/hmmalign";
	$text .= "<p>Try: $url<p>";
	curl_setopt($ch, CURLOPT_URL, $url);
#$ret = file_get_contents($url);
	$ret = curl_exec($ch);
	if ($ret == FALSE)
	{
		$text .= "lorax hmmalign was not successful<p>";
		log_error("hmmalign failed");
		return $text;
	}
	$text .= "<p>$ret<p>";
return $text;

	$url = "$lorax_url/trees/$jobkey/FastTree";
	$text .= "<p>Try: $url<p>";
	curl_setopt($ch, CURLOPT_URL, $url);
#$ret = file_get_contents($url);
	$ret = curl_exec($ch);
	if ($ret == FALSE)
	{
		$text .= "lorax FastTree was not successful<p>";
		log_error("FastTree failed");
		return $text;
	}
	$text .= "<p>$ret<p>";
	return $text;
}
#######################################################################
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

	$text = "";
	$lorax_url = variable_get('annot_loraxurl');
	$ch = curl_init();

	$sftag = "$fID.$jobkey";
	#
	# See if job is in progress
	#
	$status_url = "$lorax_url/trees/$sftag/FastTree/status";
	curl_setopt($ch, CURLOPT_URL, $status_url);
	curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1); 
	$ret = trim(curl_exec($ch));
	if ($ret == "-1")
	{
		$text .= "$refresh_text In Progress....";
		return $text;
	}
	else if ($ret == "0")  # note returns 404 if the job hasn't been started
	{
		$tree_url = "$lorax_url/trees/$sftag/FastTree/tree.nwk";
		$fname = "phytozome_10_2.$fID";

		curl_setopt($ch, CURLOPT_URL, $tree_url);
		curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1); 
		$newick_tree = trim(curl_exec($ch));

		$user_seq_names = array();
		fix_user_seq_names($newick_tree,$user_seq_names, $jobkey);
		$text .= "<p>".print_r($user_seq_names,TRUE)."<p>";
		$text .= "<p>$tree_url<p>$newick_tree<p>";

		$phylonodes = array();
		$phylonames = array();
		$phylotree_root = tripal_annot_phylotree_by_name($fname, $phylonodes, $phylonames);
		#$text .= "<pre>".print_r($phylonodes,TRUE)."</pre>";
		$t = new tripal_annot_Tree();
		$t->Parse($newick_tree);
		#$text .= "<pre>".$t->WriteNewick()."</pre>";
		$final = array();
		fill_out_tree_structure($phylonodes,$phylonames,$t, $final, $text);
		#$text .= "<pre>".print_r($final,TRUE)."</pre>";
		//populate the children[] arrays for each node
		foreach( $final as $key => &$node ) 
		{
			if($node['parent_phylonode_id'] !== 0) 
			{
				$parent_ref = &$final[ $node['parent_phylonode_id'] ]; 
				$parent_ref['children'][] = &$node; 
			}
		}
		#$text .= "<pre>".print_r($final,TRUE)."</pre>";
		$phylotree_root = &$final[1];
		$phylotree_root['phylotree_name'] = $fname;
		#$text .= "<pre>".print_r($phylotree_root,TRUE)."</pre>";

		$json = json_encode($phylotree_root);
		$text .= "<pre>$json</pre>";
		
		$endpoint = "https://localhost:8081/chado_phylotree/$fname";
		$endpoint .= "?hilite_node=".implode(",",$user_seq_names);

		$text .= "$fname<p>";
		$text .= <<<END
<script>
var treeData = 
$json;

function add_treedata()
{
	var jobj = document.getElementById("json");	
	var treestr = JSON.stringify(treeData);
	jobj.value = treestr;
}

</script>

<form  method="post" action="$endpoint" onsubmit="add_treedata()">
<input type="submit" name="submit">
<input type="hidden" id="json" name="json" value="">
</form>
END;

		return $text;
	}
	else
	{
		$httpcode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
		if ($httpcode == "404")
		{
			$text .= "Need to load the sequences and start the job!";
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
			$sumpath = tripal_annot_build_file_path($jinfo->sumname);
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
						$seqname = str_replace("|",".",$seqname); # TODO
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
					$curName = str_replace("|",".",$curName); #TODO
				}	
				else
				{
					if (isset($user_seqs[$curName]))
					{
						$user_seqs[$curName] .= trim($line);
					}
				}
			}
			$text .= "<pre>".print_r($user_seqs,TRUE)."</pre>";
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
			$text .= "<p>$gf_attach<p>";
			$url = "$lorax_url/trees/$fID.$jobkey/sequences";
			$text .= "<p>Try: $url<p>";
			curl_setopt($ch, CURLOPT_URL, $url);
			$cfile = new CURLFile($gf_attach);
			$post = array("peptide" => $cfile);
			curl_setopt($ch, CURLOPT_POSTFIELDS, $post);
			$ret = curl_exec($ch);
			if ($ret == FALSE)
			{
				$text .= "Sequence upload to lorax was not successful<p>";
				log_error("Failed to upload sequences $gf_attach to lorax server $url");
				return $text;
			}
			$url = "$lorax_url/trees/$fID.$jobkey/hmmalign_FastTree";
			$ch = curl_init();
			curl_setopt($ch, CURLOPT_URL, $url);
			curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1); 
			$ret = curl_exec($ch);
			$text .= "Launch job: $url<p>ret:$ret<p>";
			
		}
		else
		{
			$text .= "Error from $status_url:$httpcode";
		}
		return $text;

	}
	return $text;

}
function tripal_annot_phylotree_by_name($phylotree_name, &$phylonodes, &$phylonames) {
  
  //getting the name of the tree will allow construction of links to MSAs
  //TODO: might be better to have this function accept the name as its param
  $sql = <<<SQL
  SELECT phylotree_id  from chado.phylotree where name = :phylotree_name
SQL;

  $args = array(':phylotree_name' => $phylotree_name);
  $result = chado_query( $sql, $args );
  $phylotree_id = 0;
  foreach ($result as $r) {
    $phylotree_id = $r->phylotree_id;
  }

  // reminder: the chado_xxxx tables are tripal/drupal's linking tables
  $sql = <<<SQL
  SELECT n.phylonode_id, n.parent_phylonode_id, n.label AS name,
  distance AS length, f.feature_id, f.name AS feature_name,
  cvt.name AS cvterm_name, o.organism_id, o.common_name, o.abbreviation,
  o.genus, o.species, cf.nid AS feature_node_id, co.nid AS organism_node_id
  FROM chado.phylonode n
  LEFT OUTER JOIN chado.cvterm cvt ON n.type_id = cvt.cvterm_id
  LEFT OUTER JOIN chado.feature f ON n.feature_id = f.feature_id
  LEFT OUTER JOIN chado_feature cf ON cf.feature_id = f.feature_id
  LEFT OUTER JOIN chado.organism o ON f.organism_id = o.organism_id
  LEFT OUTER JOIN chado_organism co ON co.organism_id = o.organism_id
  WHERE n.phylotree_id = :phylotree_id
SQL;

  $args = array(':phylotree_id' => $phylotree_id);
  $result = chado_query( $sql, $args );

  // fetch all the phylonodes into an assoc array indexed by
  // phylonode_id.  convert into from resultset record to array,
  // fixing datatypes. chado_query returns numeric as string and fun
  // stuff like that.
  $root_phylonode_ref = null;
  
  foreach ($result as $r) {
    $phylonode_id = (int) $r->phylonode_id;

    // expect all nodes to have these properties
    $node = array(
      'phylonode_id' => $phylonode_id,
      'parent_phylonode_id' => (int) $r->parent_phylonode_id,
      'length' => (double) $r->length,
      'cvterm_name' => $r->cvterm_name
    );
    
    // other props may exist only for leaf nodes
    if($r->name) {
      $node['name'] = $r->name;
    }
    if($r->feature_id) {
      $node['feature_id'] = (int) $r->feature_id;
      $node['feature_name'] = $r->feature_name;
      $node['feature_node_id'] = (int) $r->feature_node_id;
    }
    if($r->organism_id) {
      $node['organism_id'] = (int) $r->organism_id;
      $node['common_name'] = $r->common_name;
      $node['abbreviation'] = $r->abbreviation;
      $node['genus'] = $r->genus;
      $node['species'] = $r->species;
      $node['organism_node_id'] = (int) $r->organism_node_id;
    }
    $phylonodes[$phylonode_id] = $node;
    $phylonames[$r->name] = $phylonode_id;
  }
  
  return $root_phylonode_ref;

  //populate the children[] arrays for each node
  foreach( $phylonodes as $key => &$node ) {
    if($node['parent_phylonode_id'] !== 0) {
      $parent_ref = &$phylonodes[ $node['parent_phylonode_id'] ]; // get
      $parent_ref['children'][] = &$node; // append node refernce to children
    }
    else {
      $root_phylonode_ref = &$node;
      $root_phylonode_ref['phylotree_name'] = $phylotree_name;
    }
  }
  return $root_phylonode_ref;
}
function fill_out_tree_structure(&$phylonodes, &$phylonames, $t, &$final, &$text) 
{
	# Traverse the tree structure and add data from the DB for 
	# the gene family nodes. 
	# linear array for the gene family, downloaded from the DB.
	# This means adding parent/child links for existing nodes,
	# and also adding in new nodes for the user sequences. 

	$stack = array();
	$id_stack = array(); # parallel stack, to track the newly-generated ID's
	$used_ids = array(); #TODO fill with existing ids
	$new_phylonode_id = 0;

	# Note $final has to be set up as an associative array keyed on "phylonode_id"
	# to work with the display code
	
	# depth-first traversal
	# note top of stack is always the parent of the current node
	# 
	$curnode = $t->root;
	while ($curnode != NULL)
	{	
		#$text .= implode(",",$id_stack)."<p>\n";
		if (count($stack) != count($id_stack))
		{
			die("Stack error!");
		}
		if ($curnode->GetChild())
		{
			$new_phylonode_id = new_phylonode_id($new_phylonode_id,$used_ids);
			
			if (count($stack) == 0)
			{
				# root node

				$newnode = array(
					"phylonode_id" => $new_phylonode_id,
					"parent_phylonode_id" => 0, 
					"length" => 0,
					"cvterm_name" => "phylo_root"
				);
				$final[$new_phylonode_id] = $newnode;
				
				$stack[] = $curnode;
				$id_stack[] = $new_phylonode_id;
				$curnode = $curnode->GetChild();
			}
			else
			{
				# interior node

				$length = $curnode->GetAttribute('edge_length');
				$cur_parent_id = end($id_stack);

				$newnode = array(
					"phylonode_id" => (int) $new_phylonode_id,
					"parent_phylonode_id" => (int) $cur_parent_id, 
					"length" => (double) $length,
					"cvterm_name" => "phylo_interior"
				);
				$final[$new_phylonode_id] = $newnode;
				
				$stack[] = $curnode;
				$id_stack[] = $new_phylonode_id;
				$curnode = $curnode->GetChild();
			}
		}
		else
		{
			# leaf node
			$name = $curnode->GetLabel();
			$length = $curnode->GetAttribute('edge_length');
			if ($length == '')
			{
				$length = 0; # can this happen?
			}
			$oldnode = array();
			$existing = 0;
			if (isset($phylonames[$name]))
			{
				$phylonode_id = $phylonames[$name];
				$oldnode = $phylonodes[$phylonode_id];
				$existing = 1;
			}
			else
			{
				$new_phylonode_id = new_phylonode_id($new_phylonode_id,$used_ids);
			}

			$cur_parent_id = end($id_stack);

			$newnode = array(
 				"phylonode_id" => (int) ($existing ? $phylonode_id : $new_phylonode_id),
				"parent_phylonode_id" => (int) $cur_parent_id, 
				"length" => (double) $length,
				"cvterm_name" => "phylo_leaf",
				"name" => $name,
				"feature_id" => ($existing ? $oldnode["feature_id"] : 0),
				"feature_name" => $name,
				"feature_node_id" => ($existing? $oldnode["feature_node_id"] : 0),
				"organism_id" =>  ($existing? $oldnode["organism_id"] : 0),
				"common_name" =>  ($existing? $oldnode["common_name"] : ""),
				"abbreviation" =>  ($existing? $oldnode["abbreviation"] : ""),
				"genus" =>  ($existing? $oldnode["genus"] : ""),
				"species" =>  ($existing? $oldnode["species"] : ""),
				"organism_node_id" =>  ($existing? $oldnode["organism_node_id"] : 0)
			);
			$final[$phylonode_id] = $newnode;
										
			while (!empty($stack) && ($curnode->GetSibling() == NULL))
			{
				$curnode = array_pop($stack);
				array_pop($id_stack);
			}
			if (empty($stack))
			{
				$curnode = NULL;
			}
			else
			{
				$curnode = $curnode->GetSibling();
			}
		}		
	}
}	
function new_phylonode_id($current, &$used)
{
	$current++;
	while (isset($used[$current]))
	{
		$current++;
	}
	$used[$current] = 1;
	return $current;
}
function fix_user_seq_names(&$newick_tree,&$user_seq_names, $jobkey)
{
	$matches = array();
	preg_match_all("/$jobkey\.([^:]+):/", $newick_tree, $matches);
	foreach ($matches[1] as $i => $name)
	{
		$user_seq_names[] = $name;
	}
	$newick_tree = str_replace("$jobkey.","",$newick_tree); 
}
