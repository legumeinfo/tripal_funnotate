<?php

##################################################################################
#
#	Run AHRD on the accumulated blast output files
#
#	This file also has the InterPro handler, and the HMMER handler. 
#
##################################################################################

function tripal_annot_run_ahrd($jid,$jinfo)
{
	$ymlfile = variable_get("annot_yml");
	if (!file_exists($ymlfile))
	{
		tripal_annot_log_error("Unable to find yml file: $ymlfile");
		return "Could not find yaml file!";
	}
	$yaml_obj = spyc_load_file($ymlfile); # Spyc is a YML parser 

	$upload_fpath = tripal_annot_build_query_path($jinfo);
	
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
			return "Database $dbtag in yaml file does not exist!";
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
		$outpath = tripal_annot_build_blast_output_path($jobkey,$dbnum);
		if (!file_exists($outpath))
		{
			tripal_annot_log_error("Blast output $outpath (db$i) not found!");
			return "Unable to find blast out for DB#$i";
		}
		$yaml_obj["blast_dbs"][$dbtag]["file"] = $outpath;
	}
	#annot_gzip_blasts($blasts,$jid, $jobkey);

	$yamstr = Spyc::YAMLDump($yaml_obj,2,0);

	$yamtmp = file_directory_temp()."/yaml_$jobkey.txt"; 
	file_put_contents($yamtmp,$yamstr);
	system("chmod 755 $yamtmp");

	$java_path = variable_get("annot_java_path");
	$ahrd_path = variable_get("annot_ahrd_path");
	$ahrd_cmd = "$java_path -Xmx2g -jar $ahrd_path $yamtmp";
	file_put_contents("/tmp/ahrd.txt",$ahrd_cmd); 
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
# TODO: writing props file is not entirely thread safe
function tripal_annot_run_ipr($jid,$jinfo)
{
	$iprdir = variable_get("annot_iprdir","");
	if (!file_exists("$iprdir/interproscan.sh"))
	{
		return "Can't find InterProScan!";
	}
	$iprpath = "$iprdir/interproscan.sh";
	
	$jobkey = $jinfo->jobkey;	
	$outname = "ipr_$jobkey.xml";
	$outpath = tripal_annot_build_file_path($outname);

	$iprparam = variable_get("annot_iprparam");
	$query_path = tripal_annot_build_query_path($jinfo);
	$params = " -i $query_path -o $outpath -f XML $iprparam ";
	$cmd = "$iprpath $params";
	file_put_contents("/tmp/iprcmd.txt",$cmd); # TODO

	#
	# IPR demands a properties directory
	# IF user desires multiple threads, then a props file must be configured
	# in that directory.
	# This is done on the annot config page.
	#
	$propsdir = variable_get("annot_iprcfgdir");
	#$threads = variable_get("annot_nthread",1);
	#setup_ipr_properties($propsdir,$threads);
	#system("export _JAVA_OPTIONS=-Duser.home=$propsdir; $cmd > /tmp/annotipr.txt 2>& 1" ); 
	system("export _JAVA_OPTIONS=-Duser.home=$propsdir; $cmd > /dev/null 2>& 1" );
	if (!file_exists($outpath))
	{
		return "InterPro failed - no xml output";
	}

	#
	#  Now convert xml to "raw" output, needed by AHRD
	#
	$rawname = "ipr_$jobkey.txt";
	$rawpath = tripal_annot_build_file_path($rawname);
	$cmd = "$iprpath -i $outpath -mode convert -f RAW -o $rawpath";
	#file_put_contents("/tmp/iprcmd.txt",$cmd,FILE_APPEND); 

	#system("export _JAVA_OPTIONS=-Duser.home=$propsdir; $cmd > /tmp/annotipr2.txt 2>& 1" ); 
	system("export _JAVA_OPTIONS=-Duser.home=$propsdir; $cmd > /dev/null 2>& 1" );
	if (!file_exists($rawpath))
	{
		return "Failed to convert InterPro xml to raw";
	}
	tripal_annot_update_field($jid,"iprname",$rawname);

	return "";
}

function tripal_annot_run_hmm($jid,$jinfo)
{
	$hmmdir = variable_get("annot_hmmdir","");
	$hmmsearchpath = "$hmmdir/binaries/hmmsearch";
	if (!file_exists($hmmsearchpath))
	{
		return "Can't find hmmsearch!";
	}

	$hmmdb = variable_get("annot_hmmdb");
	$hmmparam = variable_get("annot_hmmparam");
	
	$jobkey = $jinfo->jobkey;
	$outname = "hmm_$jobkey.tbl";
	$outpath = tripal_annot_build_file_path($outname);
	$query_path = tripal_annot_build_query_path($jinfo);
	$threads = variable_get("annot_nthread",1);
	$params = " --cpu $threads --tblout $outpath $hmmdb $query_path "; # TODO: cpu not working?
	$cmd = "$hmmsearchpath $params";

	#file_put_contents("/tmp/hmmcmd.txt",$cmd); 
	#system("$cmd > /tmp/annothmm.txt 2>& 1" );
	system("$cmd > /dev/null 2>& 1" );
	if (!file_exists($outpath))
	{
		#file_put_contents($outpath,""); # TODO: does HMMer generate a file if no results?
		return "HMMer failed: no output";   
	}
	else
	{
		tripal_annot_update_field($jid,"hmmname",basename($outpath));
	}
	return "";

}
