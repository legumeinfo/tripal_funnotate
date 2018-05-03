<?php
require_once("tripal_annot.usertree.inc");

error_reporting(E_ALL);
#####################################################################################
#
# Job tracking page: Track job status and provide results for download
# When used with GET parameters, also initiates tree computation and MSA retrieval
#
#######################################################################################

function annot_job_page($jobkey)
{
#die(drupal_get_path("module","tripal_annot"));
 

	if (isset($_GET["msa"]))
	{
		# request for lorax msa, should be msa=family_id
		return annot_get_msa($jobkey,$_GET["msa"]);
	}
	else if (isset($_GET["family"] ))
	{
		#return annot_tree_test();
		$url = variable_get('annot_loraxurl','');
		if ($url == "")
		{
			return "<p>Unable to create gene family tree since no lorax server was configured<p>";
		}
		$fID = $_GET["family"];
		if (!preg_match('/^[A-z_0-9]+$/',$fID))
		{
			return "<p>Invalid gene family!<p>";
		}
		return tripal_annot_family_tree($jobkey,$fID);
	}
#drupal_add_css("https://cdn.datatables.net/v/dt/jq-2.2.4/dt-1.10.13/cr-1.3.2/fc-3.2.2/fh-3.1.2/sc-1.4.2/se-1.2.0/datatables.min.css");
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
	$skipipr = $jarr["skip_ipro"];

	$basetype = ($seqtype == "n" ? "basepairs" : "amino acids");
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
<br>
BLAST output files: $blast_file_links
END;

	if ($skipipr == 0)
	{
		$text .= <<<END
<br>
<a href="$url_ipr" style="text-decoration:underline">InterPro output</a> (txt file, RAW format)
END;
	}
	$text .= <<<END
<br>
<a href="$url_hmm" style="text-decoration:underline">HMM output</a> (txt file,tabular)
<br>
<a href="$url_ahrd" style="text-decoration:underline">AHRD output</a> (txt file,tabular)
<p>
Summary Table 
(right-click <a href="$url_sum" style="text-decoration:underline">download</a>) :
END;
	if ($skipipr == 1)
	{
		#$text .= "<br>(Note that InterPro was not run, hence GO and InterPro data fields are empty)<br>\n";
	}
	$text .= <<<END
<script>
var oldJQ = jQuery;
</script>
<script type="text/javascript" src="https://cdn.datatables.net/v/dt/jq-2.2.4/dt-1.10.13/cr-1.3.2/fc-3.2.2/fh-3.1.2/sc-1.4.2/se-1.2.0/datatables.min.js"></script>
<script>
function loadjscssfile(filename, filetype){
    if (filetype=="js"){ //if filename is a external JavaScript file
        var fileref=document.createElement('script')
        fileref.setAttribute("type","text/javascript")
        fileref.setAttribute("src", filename)
    }
    else if (filetype=="css"){ //if filename is an external CSS file
        var fileref=document.createElement("link")
        fileref.setAttribute("rel", "stylesheet")
        fileref.setAttribute("type", "text/css")
        fileref.setAttribute("href", filename)
    }
    if (typeof fileref!="undefined")
        document.getElementsByTagName("head")[0].appendChild(fileref)
}
loadjscssfile("https://cdn.datatables.net/v/dt/jq-2.2.4/dt-1.10.13/cr-1.3.2/fc-3.2.2/fh-3.1.2/sc-1.4.2/se-1.2.0/datatables.min.css", "css");
var jquery224 = jQuery.noConflict(true); 
(function($) {                                                                  
    var jQuery = $;                                                             
    $(document).ready( function ($) {                                           
    	$('#result_tbl').DataTable({colReorder: true});
    });                                                                         
})(jquery224);     
</script>
<table id='result_tbl' class='display compact'>
END;

$fpath = tripal_annot_build_file_path($sumname);
$fh = fopen($fpath,"r");
$linenum = 0;
$tool_icon_tag = "<img src='".file_create_url("public://images/tools-512.png")."' width=15>";
while ( ($line = fgets($fh)) != FALSE)
{
	if (!preg_match('/\S/',$line))
	{
		continue;
	}
	$linenum++;
	$fields = preg_split("/\t/",$line);
	if ($linenum == 1)
	{
		#$fields[2] = "AHRD&nbsp;Quality<br><a href='#quality' style='text-decoration:underline'>explain</a>";
		$fields[2] = "AHRD&nbsp;Quality<sup>3</sup>";
		$fields[7] = "GF&nbsp;Score<sup>1</sup>";
		$fields[9] = "Blast&nbsp;Score<sup>2</sup>";
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
		if (trim($fields[6]) != "") # written for multiple possible families but only one so far
		{
			$gfs = explode(",",$fields[6]);
			$linkouts = array();
			foreach ($gfs as $i => $gf)
			{
                #FIXME: should either make the prefix configurable or else just mandate that the hmm file includes the fully namespaced identifiers; but this is no worse than what we had previously hard-coded with phytozome families
				$linkout1 = "<a href=\"/chado_phylotree/legfed_v1_0.$gf\" title='View the phylotree for this family'>$gf</a>";
				$linkout2 = "<a href=\"/annot/job/$jobkey?family=$gf\" target=\"_blank\" title='Rebuild family phylotree including your sequence'>$tool_icon_tag</a>";
				$linkout = "<span style='white-space:nowrap'>$linkout1&nbsp;$linkout2</span>";
				if ($i % 4 == 3)
				{
					$linkout .= "<br>";
				}
				array_push($linkouts,$linkout);
			}
			$fields[6] = implode("&nbsp;",$linkouts);
		}
	}

	#
	# If the user did not run ipr, remove those columns from the table. 
	#
	if ($skipipr == 1)
	{
		unset($fields[4]);
		unset($fields[5]);
	}

	if ($linenum == 1)
	{
		$row = "<tr><th>".implode("</th><th>",$fields)."</th></tr>\n";
		$row = "<thead>\n$row\n</thead>\n<tbody>\n";
	}
	else
	{
		$row = "<tr><td>".implode("</td><td>",$fields)."</td></tr>\n";
	}
	$text .= $row;
}
$text .= "\n</tbody>\n</table>\n";
$text .= "<sup>1</sup> GF Score = full-sequence E-value from hmmscan<br>";
$text .= "<sup>2</sup> Blast Score: E-value is displayed, however best hit is chosen using bit score <br>";

$text .= <<<END
<a name="quality"></a>
<sup>3</sup> AHRD Quality Scores are as follows:
<p>
<pre>
The AHRD quality-code consists of a three character string, where each 
character is either "*" if the respective criteria is met or "_" otherwise. 
The meaning by position is as follows:

1	Bit score of the blast result is over 50 and e-value is better than e-10
2	Overlap of the blast result is at least 60%
3	Top token score of assigned descriptor is at least 0.5

Note that the Best Blast Hit may differ from the AHRD Blast Hit because AHRD factors 
in the information content of the sequence description. 

For further explanation of these codes and the AHRD algorithm, see
<a href="https://github.com/groupschoof/AHRD/blob/master/README.textile">AHRD Documentation</a>.
</pre>
END;
$run_secs = $end_time - $start_time;
$run_mins = floor($run_secs/60);
$run_secs -= 60*$run_mins;
$text .= "<span style='font-size:11px'>job duration:$run_mins minutes, $run_secs seconds</span><br>"; 

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
		if ( count($f) != 10) 
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
function annot_get_msa($jobkey, $fID)
{
	$jinfo = tripal_annot_get_job_info(0,$jobkey);
	if ($jinfo == FALSE)
	{
		return "";
	}
	$sftag = "$fID.$jobkey";
	$lorax_url = variable_get('annot_loraxurl');
	$ch = curl_init();
	$msa_url = "$lorax_url/trees/$sftag/alignment";
	curl_setopt($ch, CURLOPT_URL, $msa_url);
	curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1); 
	$msa = trim(curl_exec($ch));
	$lines = explode("\n",$msa);
	$msa = "";
	$matches = array();
	$seqs = array();
	$curseq = "";
	foreach ($lines as $line)
	{
		if (preg_match('/^>(\S+)/',$line,$matches))
		{
			#$msa .= ">".$matches[1]."\n";
			$curseq = $matches[1];
		}
		else
		{
			$seqs[$curseq] .= trim($line);
		}
	}

	$job_prefix = $jobkey.".";
	foreach ($seqs as $curseq => $seq)
	{
		if (0 == strncmp($curseq,$job_prefix,strlen($job_prefix)))
		{
			$curseq = substr($curseq,strlen($job_prefix));
			$curseq = "USR.$curseq";
		}
		$msa .= ">$curseq\n";
		for ($i = 0; $i < strlen($seq); $i += 50)
		{
			$msa .= substr($seq,$i,50)."\n";
		}
	}
	#$tmpfile = "/tmp/$jobkey.msa.txt";
	#file_put_contents($tmpfile,$msa);
	$size = strlen($msa); #filesize($tmpfile);
	drupal_add_http_header('Content-Type', 'text/plain');
	drupal_add_http_header('Content-Length', $size);
	#drupal_add_http_header('Content-Disposition', "attachment; filename='msa.txt'");
	echo $msa;
	drupal_exit();
	return "";
}
