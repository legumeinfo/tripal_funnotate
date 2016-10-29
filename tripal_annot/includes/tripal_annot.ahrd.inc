<?php

##################################################################################
#
#	Run AHRD on the accumulated blast output files
#
#	This file also has the InterPro handler, and the HMMER handler. 
#
##################################################################################

# Note that all files which get added to file_managed are named using the unique "jobkey"
# This is because the managed file system does not allow duplicated filenames
# and if a duplicate is added it will fail with no error message

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
	tripal_annot_update_field($jid,	"ahrdfid",$fid);
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
	$res = db_query("select ultype,jobkey from tripal_annot where jid=$jid");
	if (!$res)
	{
		tripal_annot_append_job_error("Could not find job $jid in tripal_annot table!");
		return;
	}	
	$r = $res->fetchObject();			
	$seqtype = $r->ultype;	
	$jobkey = $r->jobkey;	

	$propsdir = variable_get("annot_iprcfgdir");
	file_put_contents("/tmp/cmd1.txt",$cmd);
	system("export _JAVA_OPTIONS=-Duser.home=$propsdir; $cmd > /tmp/annotipr.txt 2>& 1" );
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
	if (!file_exists($rawpath))
	{
		tripal_append_job_error("jid","Failed to convert InterPro xml to raw");
		return;
	}
	$fid = annot_add_managed_file($rawname);
	tripal_annot_update_field($jid,"iprfid",$fid);


}

function run_tripal_annot_hmm($cmd, $outpath, $jid)
{
	$res = db_query("select ultype,jobkey from tripal_annot where jid=$jid");
	if (!$res)
	{
		tripal_annot_append_job_error("Could not find job $jid in tripal_annot table!");
		return;
	}	
	$r = $res->fetchObject();			
	$jobkey = $r->jobkey;	

	file_put_contents("/tmp/cmd3.txt",$cmd);
	system("$cmd > /tmp/annothmm.txt 2>& 1" );
	if (!file_exists($outpath))
	{
		tripal_append_job_error("jid","Failed to create HMMER result $outpath");
		return;
	}
	$fid = annot_add_managed_file(basename($outpath));
	tripal_annot_update_field($jid,"hmmfid",$fid);

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
