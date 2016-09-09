<?php

##################################################################################
#
#	Run AHRD on the accumulated blast output files
#	and do other postprocessing (if any) to finish the job
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
	$ahrd_outpath = DRUPAL_ROOT."/".tripal_get_files_dir("annot")."/$ahrd_outname";
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
		$outpath = DRUPAL_ROOT."/".tripal_get_files_dir("annot")."/$outname";
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
	$outdir = DRUPAL_ROOT."/".tripal_get_files_dir("annot");
	$outpath = DRUPAL_ROOT."/".tripal_get_files_dir("annot")."/$outname";
	
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


