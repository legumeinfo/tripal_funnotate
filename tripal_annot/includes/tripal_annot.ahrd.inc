<?php

##################################################################################
#
#	Run AHRD on the accumulated blast output files
#	and do other postprocessing (if any) to finish the job.
#	This file also has the InterPro handler.
#
##################################################################################

#
#	Callback function for running AHRD
#
function run_tripal_annot_ahrd($dblist, $jid)
{
	$dbs = explode(",",$dblist);
	if (count($dbs) == 0)
	{
		# This should be caught earlier but anyway
		tripal_annot_append_job_error( "No annotation blasts were run (job $jid, dblist $dblist)!");
		return;
	}
	$ymlfile = variable_get("annot_yml");
	if (!file_exists($ymlfile))
	{
		tripal_annot_append_job_error("Could not open yaml file $ymlfile!");
		return;
	}
	$yaml_obj = spyc_load_file($ymlfile);
	$res = db_query("select * from tripal_annot where jid=$jid");
	if (!$res)
	{
		tripal_annot_append_job_error("Could not find job $jid in tripal_annot table!");
		return;
		
	}	
	$r = $res->fetchObject();			
	$upload_fobj =	file_load($r->ulfid); 		
	$upload_fpath = drupal_realpath($upload_fobj->uri);
	$jobkey = $r->jobkey;

	$yaml_obj["proteins_fasta"] = $upload_fpath;
	$ahrd_outname = "ahrd_$jobkey.txt";
	$ahrd_outpath = tripal_annot_build_file_path($ahrd_outname);
	$yaml_obj["output"] = $ahrd_outpath;

	if ($r->ultype == "n")
	{
		$yaml_obj["token_score_bit_score_weight"] = 0.6;
		$yaml_obj["token_score_database_score_weight"] = 0.4;
		$yaml_obj["token_score_overlap_score_weight"] = 0;
	}
	
	$blasts = array();
	foreach ($yaml_obj["blast_dbs"] as $dbtag => $dbarr)
	{
		$dbpath = $dbarr["database"];
		if ($dbpath == "" || !file_exists($dbpath))
		{
			tripal_annot_append_job_error("Database $dbpath in yaml $ymlfile does not exist!");
			return;
		}
		# We have to find which of the databases this one is
		# in order to know the blast output file name
		$dbnum = 0;
		for ($i = 1; $i <= 4; $i++)
		{
			$dbpath2 = variable_get("annot_db$i");
			if ($dbpath2 == $dbpath)
			{
				$dbnum = $i;
				break;
			}
		}
		$outname = "blast_$jid"."_$dbnum";
		$outpath = tripal_annot_build_file_path($outname);
		if (!file_exists($outpath))
		{
			tripal_annot_append_error("Blast output $outpath (db$i) not found!");
			return;
		}
		array_push($blasts,$outname);
		$yaml_obj["blast_dbs"][$dbtag]["file"] = $outpath;
	}
	annot_gzip_blasts($blasts,$jid, $jobkey);

	$yamstr = Spyc::YAMLDump($yaml_obj,2,0);
	file_put_contents("/tmp/yamldump.txt",$yamstr);
	system("chmod 777 /tmp/yamldump.txt");

	$java_path = variable_get("annot_java_path");
	$ahrd_path = variable_get("annot_ahrd_path");
	$ahrd_cmd = "$java_path -Xmx2g -jar $ahrd_path /tmp/yamldump.txt";
	file_put_contents("/tmp/cmd.txt",$ahrd_cmd);
	system("chmod 777 /tmp/cmd.txt");	
	system($ahrd_cmd);	

	if (!file_exists($outpath))
	{
		tripal_append_job_error("jid","AHRD failed! (no output file)");
		return;
	}
	$fid = annot_add_managed_file($ahrd_outname);

	tripal_annot_update_field($jid,	"outfid",$fid);
}
function annot_gzip_blasts(&$blasts, $jid, $jobkey)
{
	$blaststr = implode(" ",$blasts);
	$outname = "blasts_$jobkey.tar.gz";
	$outdir = tripal_annot_file_dir();
	$outpath = tripal_annot_build_file_path($outname);
	
	$curdir = getcwd();
	chdir($outdir);
	$cmd = "tar cfz $outname $blaststr";
	system($cmd);
	chdir($curdir); # do it here so if we return we're in the right place
	if (!file_exists($outpath))
	{
		tripal_append_job_error("jid","Failed to create blast gzip $outpath");
		return;
	}
	$fid = annot_add_managed_file($outname);
	tripal_annot_update_field($jid,"blastfid",$fid);
}


#
#	Callback function for running InterPro
#
function run_tripal_annot_ipr($cmd, $outpath, $jid)
{
	# We need to know if it's a nucleotide seq since IPR appends to the name
	$res = db_query("select ultype from tripal_annot where jid=$jid");
	if (!$res)
	{
		tripal_annot_append_job_error("Could not find job $jid in tripal_annot table!");
		return;
	}	
	$r = $res->fetchObject();			
	$seqtype = $r->ultype;	

	$propsdir = variable_get("annot_iprcfgdir");
	file_put_contents("/tmp/cmd1.txt",$cmd);
	system("export _JAVA_OPTIONS=-Duser.home=$propsdir; $cmd > /tmp/annotipr.txt 2>& 1" );
	if (!file_exists($outpath))
	{
		tripal_append_job_error("jid","Failed to create InterPro result $outpath");
		return;
	}

	$rawname = "ipr_$jid.txt";
	$rawpath = tripal_annot_build_file_path($rawname);
	$iprdir = variable_get("annot_iprdir","");
	if (!file_exists("$iprdir/interproscan.sh"))
	{
		tripal_append_job_error("jid","Can't find interproscan.sh in $iprdir");
		return;
	}
	$iprpath = "$iprdir/interproscan.sh";
	$cmd = "$iprpath -i $outpath -mode convert -f RAW -o $rawpath";
	file_put_contents("/tmp/cmd.txt",$cmd);
	system("export _JAVA_OPTIONS=-Duser.home=$propsdir; $cmd > /tmp/annotipr2.txt 2>& 1" );
	if (!file_exists($rawpath))
	{
		tripal_append_job_error("jid","Failed to convert InterPro xml to raw");
		return;
	}

	$fid = annot_add_managed_file($rawname);
	tripal_annot_update_field($jid,"iprfid",$fid);

	# Scan the IPR output and get IDs and GOs from the best hits	

	$txt = "";
	
	$fh = fopen($rawpath,"r");
	$bestiprs = array();
	$bestgos = array();
	$bestevals = array();
	while ( ($line = fgets($fh)) != FALSE)
	{
		$fields = preg_split("/\t/",$line);
		$query = $fields[0];
		if ($seqtype == "n")
		{
			# IPR adds _NN to indicate the ORF number; must remove
			$query = preg_replace('/_\d+$/','',$query);
		}
		$eval = $fields[8];
		if (is_numeric($eval))
		{
			$iprid = $fields[11];
			if ($iprid == "NULL")
			{
				$iprid = "";
			}
			$goline = $fields[13];
			if (!isset($bestevals[$query]) || $eval < $bestevals[$query])
			{
$txt .= "\n\n$query\t$eval (prior eval:".$bestevals[$query].")\n";
				$bestevals[$query] = $eval;
				$bestiprs[$query] = $iprid;
				$matches = array();
				$nummatch = preg_match_all("|GO:(\d+)|",$goline,$matches);
$txt .= print_r($matches,true);
				$gostr = ($nummatch > 0 ? implode(";",$matches[1]) : "");	
				$bestgos[$query] = $gostr;
$txt .= "$iprid\tGOS:$gostr\n";
			}
		}
	}
	fclose($fh);
$txt .= "\n****************************************\nIPRID\n".print_r($bestiprs,TRUE);
$txt .= "\n****************************************\nGOS\n".print_r($bestgos,TRUE);
file_put_contents("/tmp/iprparse.txt",$txt);

	# Now integrate the IPR IDs and GOs into the AHRD output
	$result = db_query("select outfid from tripal_annot where jid=$jid");
	if ($result->rowCount() == 0)
	{
		tripal_append_job_error("jid","InterPro: could not find database entry for job $jid");
		return;
	}

	$jarr = $result->fetchAssoc();
	$ahrd_fid = $jarr["outfid"];
	$ahrd_fobj = file_load($ahrd_fid);
	$ahrd_path = drupal_realpath($ahrd_fobj->uri);

	$fh1 = fopen($ahrd_path,"r");
	$fh2 = fopen("/tmp/ahrd_fix.txt","w");

	$got_header == 0;
system("rm /tmp/txtmatch.txt");
	while ( ($line = fgets($fh1)) != FALSE)
	{
		if ($got_header < 3)
		{
			fwrite($fh2,$line);
			$got_header++;
			continue;
		}
file_put_contents("/tmp/txtmatch.txt","AHRD:$line"."\n",FILE_APPEND);
		$fields = preg_split("/\t/",$line);
		$query = trim($fields[0]);
		$ipr_query = find_matching_query($query,$bestiprs); 
		$newline = trim($line);
		if ($ipr_query != "")
		{
			$newline .= "\t".$bestiprs[$ipr_query]."\t".$bestgos[$ipr_query];
		}
		else
		{
			$newline .= "\t\t";
		}
		fwrite($fh2,"$newline\n");
	}
	fclose($fh1);
	fclose($fh2);

	system("cp $ahrd_path /tmp/ahrd_orig.txt");
	system("cp /tmp/ahrd_fix.txt $ahrd_path");
}
#
# Unfortunately some tools convert uniprot fasta headers like sp|Q197F8|002R_IIV3
# to shortened names like Q197F8 .
# Interpro does this, hence we have to do a special check to match the AHRD query names, 
# which maintain the full query name. 
# $longname : from AHRD output
# $shortname : in InterPro output
#
function find_matching_query($longname,&$shortname_array)
{
$txt = "";
$txt .= "try to match $longname\n";
	if (isset($shortname_array[$longname]))
	{
$txt .= "matched $longname!\n";
file_put_contents("/tmp/txtmatch.txt",$txt,FILE_APPEND);
		return $longname;
	}
	foreach ($shortname_array as $shortname => $foo)
	{
$txt .= "try to match $shortname in $longname\n";
		if (FALSE != strstr($longname,"|$shortname|")) # not perfect match but should be good enough
		{
$txt .= "matched $shortname to $longname!\n";
file_put_contents("/tmp/txtmatch.txt",$txt,FILE_APPEND);
			return $shortname;
		}
	}
$txt .= "no match!\n";
file_put_contents("/tmp/txtmatch.txt",$txt,FILE_APPEND);
	return "";
}

