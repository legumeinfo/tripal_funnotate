<?php

##################################################################################
#
# Postprocessing job callback to create final output summary 
#
##################################################################################

#
# TODO: column #'s of summary table are hard coded here and in jobtrack.inc
#

function tripal_annot_run_post($jid,$jinfo)
{
	$seqtype = $jinfo->ultype;	
	$jobkey = $jinfo->jobkey;	
	$ahrdname = $jinfo->ahrdname;	
	$iprname = $jinfo->iprname;	
	$hmmname = $jinfo->hmmname;	

	# Scan the IPR output and collect IDs and GOs from ALL the hits that have IDs, since it appears that 
	# all of these should be regarded as significant

	$alliprs = array();
	$allgos = array();

	if ($jinfo->skip_ipro == 0)
	{
		$ipr_path = tripal_annot_build_file_path($iprname);
		$fh = fopen($ipr_path,"r");
		if ($fh == FALSE)
		{
			return "Failed to open InterPro output";
		}
		while ( ($line = fgets($fh)) != FALSE)
		{
			$fields = preg_split("/\t/",$line);
			$query = $fields[0];
			if (0) #$seqtype == "n") # no longer needed since we use the translated query
			{
				# IPR adds _NN to indicate the ORF number; must remove
				$query = preg_replace('/_\d+$/','',$query);
			}
			$eval = $fields[8];
			if (!is_numeric($eval))
			{
				$eval = 1; 
			}
			$iprid = $fields[11];
			$iprdesc = trim($fields[12]);
			if ($iprid == "NULL")
			{
				$iprid = "";
			}
			$goline = $fields[13];
			$matches = array();
			$nummatch = preg_match_all("|GO:(\d+)|",$goline,$matches);
			if ($iprid != "")
			{
				$alliprs[$query][$iprid] = "$iprid"; # ($iprdesc)";
				foreach ($matches[1] as $go)
				{
					$allgos[$query][$go] = $go;
				}
			}
		}
	}
	fclose($fh);

	#
	# Load the HMM output, if any
	#
	$best_hmm_hit = array();
	if ($hmmname != "")
	{
		$hmm_path = tripal_annot_build_file_path($hmmname);
		$fh = fopen($hmm_path,"r");
		if ($fh == FALSE)
		{
			return "Failed to open HMMer output";
		}
		$best_hmm_eval = array();
		$best_hmm_score = array();
		while ( ($line = fgets($fh)) != FALSE)
		{
			if (substr($line,0,1) == "#") continue;
			$fields = preg_split("/\s+/",$line);
			$query = $fields[0];
			$target = $fields[2];
			$evalue = $fields[4];
			$score = $fields[5];
			if (!isset($best_hmm_hit[$query]))
			{
				$best_hmm_hit[$query] = $target;
				$best_hmm_eval[$query] = $eval;
				$best_hmm_score[$query] = $score;
			}
			else
			{
				if ($eval < $best_hmm_eval[$query] ||
					($eval == $best_hmm_eval[$query] && $score > $best_hmm_score[$query]))
				{
					$best_hmm_hit[$query] = $target;
					$best_hmm_eval[$query] = $eval;
					$best_hmm_score[$query] = $score;
				}
			}
		}
	}

	#
	# Create the summary output file
	#

	$ahrd_path = tripal_annot_build_file_path($ahrdname);
	$sum_name = "summary_$jobkey.txt";
	$sum_path = tripal_annot_build_file_path($sum_name);

	$fh1 = fopen($ahrd_path,"r");
	if ($fh1 == FALSE)
	{
		return "Failed to open AHRD output";
	}
	$fh2 = fopen($sum_path,"w");
	if ($fh2 == FALSE)
	{
		return "Unable to create summary output file!";
	}

	fwrite($fh2,"Query\tAHRD Blast Hit\tAHRD Qual\tAHRD Descriptor\tInterpro-ID\tGO Terms\tGene Family\n");
	$got_header == 0;
	while ( ($line = fgets($fh1)) != FALSE)
	{
		if ($got_header < 3)
		{
			$got_header++;
			continue;
		}
		$fields = preg_split("/\t/",$line);
		$query = trim($fields[0]);
		$hit = trim($fields[1]);
		$qual = trim($fields[2]);
		$desc = trim($fields[3]);
		$ipr_query = find_matching_query($query,$alliprs); 
		$newline = "$query\t$hit\t$qual\t$desc\t";
		if ($ipr_query != "")
		{
			$newline .= implode(",",$alliprs[$ipr_query]);
			$newline .= "\t";
			if (isset($allgos[$ipr_query]))
			{
				$newline .= implode(",",$allgos[$ipr_query]);
			}
			$newline .= "\t";
		}
		else
		{
			$newline .= "\t\t";
		}
		if (isset($best_hmm_hit[$query]))
		{
			$newline .= $best_hmm_hit[$query];
		}
		fwrite($fh2,"$newline\n");
	}
	fclose($fh1);
	fclose($fh2);

	tripal_annot_update_field($jid,"sumname",$sum_name);
	return "";
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
	if (isset($shortname_array[$longname]))
	{
		return $longname;
	}
	foreach ($shortname_array as $shortname => $foo)
	{
		if (FALSE != strstr($longname,"|$shortname|")) # not perfect match but should be good enough
		{
			return $shortname;
		}
	}
	return "";
}
