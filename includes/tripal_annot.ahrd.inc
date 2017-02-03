<?php

##################################################################################
#
#	Run AHRD on the accumulated blast output files
#
#	This file also has the InterPro handler, and the HMMER handler. 
#
##################################################################################

function run_tripal_annot_ahrd($jid,$jinfo)
{
	$ymlfile = variable_get("annot_yml");
	if (!file_exists($ymlfile))
	{
		tripal_annot_log_error("Unable to find yml file: $ymlfile");
		return "Could not find yaml file!");
	}
	$yaml_obj = spyc_load_file($ymlfile); # Spyc is a YML parser 

	$upload_fpath = tripal_annot_build_query_path();
	
	$jobkey = $jinfo->jobkey;
	$ahrd_outname = "ahrd_$jobkey.txt";
	$ahrd_outpath = tripal_annot_build_file_path($ahrd_outname);

	#
	# edit the yml data to put in the specific blast output paths, etc
	#
	$yaml_obj["proteins_fasta"] = $upload_fpath;
	$yaml_obj["output"] = $ahrd_outpath;

	$blasts = array();
	foreach ($yaml_obj["blast_dbs"] as $dbtag => $dbarr)
	{
		$dbpath = $dbarr["database"];
		if ($dbpath == "" || !file_exists($dbpath))
		{
			tripal_annot_log_error("Database $dbpath in yaml $ymlfile does not exist!");
			return "Database $dbtag in yaml file does not exist!");
		}

		# We have to find the numeral of this database,
		# in order to know the blast output file name
		#
		$dbnum = 0;
		for ($i = 1; $i <= 4; $i++) # TODO: hardcoded at 4
		{
			$dbpath2 = variable_get("annot_db$i");
			if ($dbpath2 == $dbpath) # exact string match: but this was already checked during config
			{
				$dbnum = $i;
				break;
			}
		}
		$outpath = tripal_annot_build_blast_output_path($jid,$dbnum);
		if (!file_exists($outpath))
		{
			tripal_annot_log_error("Blast output $outpath (db$i) not found!");
			return "Unable to find blast out for DB#$i";
		}
		$yaml_obj["blast_dbs"][$dbtag]["file"] = $outpath;
	}
	#annot_gzip_blasts($blasts,$jid, $jobkey);

	$yamstr = Spyc::YAMLDump($yaml_obj,2,0);

	$yamtmp = file_directory_temp()."/yamldump.txt"; # TODO: remove
	file_put_contents($yamtmp,$yamstr);
	system("chmod 755 $yamtmp");

	$java_path = variable_get("annot_java_path");
	$ahrd_path = variable_get("annot_ahrd_path");
	$ahrd_cmd = "$java_path -Xmx2g -jar $ahrd_path $yamtmp";
	##file_put_contents("/tmp/ahrd.txt",$ahrd_cmd);
	system($ahrd_cmd);	

	if (!file_exists($outpath))
	{
		return "AHRD failed! (no output file)";
	}
	tripal_annot_update_field($jid,	"ahrdname",$ahrd_outname);
	return "";
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
	tripal_annot_update_field($jid,"blastname",$outname);
}


#
#	Callback function for running InterPro
#
function run_tripal_annot_ipr($cmd, $outpath, $jid)
{
	$iprdir = variable_get("annot_iprdir","");
	if (!file_exists("$iprdir/interproscan.sh"))
	{
		drupal_set_message("Can't find interproscan.sh in $iprdir!");
		return 0;
	}
	$iprpath = "$iprdir/interproscan.sh";

	#$tparam = ($type == "n" ? " -t n " : "");
	

	$iprparam = variable_get("annot_iprparam");
	
	$outname = "ipr_$jid.xml";
	$outpath = tripal_annot_build_file_path($outname);
	$params = " -i $translated_path -o $outpath -f XML $iprparam ";
	file_put_contents("/tmp/ipr.txt",$cmd);
	$res = db_query("select jobkey from tripal_annot where jid=$jid");
	if (!$res)
	{
		tripal_annot_append_job_error("Could not find job $jid in tripal_annot table!");
		return;
	}	
	$r = $res->fetchObject();			
	$jobkey = $r->jobkey;	

	$propsdir = variable_get("annot_iprcfgdir");
	system("export _JAVA_OPTIONS=-Duser.home=$propsdir; $cmd > /tmp/annotipr.txt 2>& 1" );
	##system("export _JAVA_OPTIONS=-Duser.home=$propsdir; $cmd > /dev/null 2>& 1" );
	if (!file_exists($outpath))
	{
		tripal_append_job_error("jid","Failed to create InterPro result $outpath");
		return;
	}

	$rawname = "ipr_$jobkey.txt";
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
	system("export _JAVA_OPTIONS=-Duser.home=$propsdir; $cmd > /dev/null 2>& 1" );
	if (!file_exists($rawpath))
	{
		tripal_append_job_error("jid","Failed to convert InterPro xml to raw");
		return;
	}
	tripal_annot_update_field($jid,"iprname",$rawname);


}

function run_tripal_annot_hmm($cmd, $outpath, $jid)
{
	$hmmdir = variable_get("annot_hmmdir","");
	if (!file_exists("$hmmdir/binaries/hmmsearch"))
	{
		drupal_set_message("Can't find hmmsearch in $hmmdir/binaries!");
		return 0;
	}
	$hmmsearchpath = "$hmmdir/binaries/hmmsearch";

	$hmmdb = variable_get("annot_hmmdb");
	$hmmparam = variable_get("annot_hmmparam");
	
	$outname = "hmm_$jobkey.tbl";
	$outpath = tripal_annot_build_file_path($outname);
	$params = " --tblout $outpath $hmmdb $translated_path ";
	$cmd = "$hmmsearchpath $params";
	$res = db_query("select jobkey from tripal_annot where jid=$jid");
	if (!$res)
	{
		tripal_annot_append_job_error("Could not find job $jid in tripal_annot table!");
		return;
	}	
	$r = $res->fetchObject();			
	$jobkey = $r->jobkey;	

	##file_put_contents("/tmp/cmd3.txt",$cmd);
	##system("$cmd > /tmp/annothmm.txt 2>& 1" );
	system("$cmd > /dev/null 2>& 1" );
	if (!file_exists($outpath))
	{
		tripal_append_job_error("jid","Failed to create HMMER result $outpath");
		return;
	}
	tripal_annot_update_field($jid,"hmmname",basename($outpath));

	$fh = fopen($outpath,"r");
	$besthit = array();
	$besteval = array();
	$bestscore = array();
	while ( ($line = fgets($fh)) != FALSE)
	{
		if (substr($line,0,1) == "#") continue;
		$fields = preg_split("/\t/",$line);
		$query = $fields[0];
		$target = $fields[2];
		$evalue = $fields[4];
		$score = $fields[5];
		if (!isset($besthit[$query]))
		{
			$besthit[$query] = $target;
			$besteval[$query] = $eval;
			$bestscore[$query] = $score;
		}
		else
		{
			if ($eval < $besteval[$query] ||
				($eval == $besteval[$query] && $score > $bestscore[$query]))
			{
				$besthit[$query] = $target;
				$besteval[$query] = $eval;
				$bestscore[$query] = $score;
			}
		}
	}
}
