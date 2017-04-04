<?php
require_once("tripal_annot.tree.inc");

error_reporting(E_ALL);
#####################################################################################
#
# 	Handle generation and display of trees with user sequences embedded 
#	in existing gene families.
#	Generation is by calling lorax server, which must be available.
#	Display is by posting tree data to the endpoint exposed in tripal_phylotree.
#
#######################################################################################

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
	 if(new Date().getTime() - time >= 10000) 
		 window.location.reload(true);
	 else 
		 setTimeout(refresh, 10000);
}

setTimeout(refresh, 10000);
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
		$text .= $refresh_text;
		#$text .= "<span style='color:red'>Development in progress, functionality may be temporarily impaired</span><p>\n";
		$text .= "<p>Computing phylogenetic tree, please be patient...<p>";
		return $text;
	}
	else if ($ret == "0")  # done
	{
		#$text .= "<span style='color:red'>Development in progress, functionality may be temporarily impaired</span><p>\n";
		$tree_url = "$lorax_url/trees/$sftag/FastTree/tree.nwk";
		$fname = "phytozome_10_2.$fID";

		curl_setopt($ch, CURLOPT_URL, $tree_url);
		curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1); 
		$newick_tree = trim(curl_exec($ch));
		#$text .= "<p>$tree_url<p>$newick_tree<p>";

		$user_seq_names = array();
		fix_user_seq_names($newick_tree,$user_seq_names, $jobkey);
		#$text .= "<p>".print_r($user_seq_names,TRUE)."<p>";

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

		#populate the children[] arrays for each node
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
		#$text .= "<pre>$json</pre>";
		
		$endpoint = "/chado_phylotree/$fname";
		$endpoint .= "?hilite_node=".implode(",",$user_seq_names);
		$msa_endpoint = "/annot/job/$jobkey?msa=$fID";

		# The tree data is posted to the display endpoint using an automatically submitted
		# form. Before submission, the tree array is written into a hidden form
		# variable using javascript. This gets the encoding right for upload. 

		$text .= <<<END
<script>
var msaURL = '$msa_endpoint';
var treeData = 
$json;

function add_treedata()
{
	var jobj = document.getElementById("json");	
	var treestr = JSON.stringify(treeData);
	jobj.value = treestr;
	jobj = document.getElementById("msa");	
	jobj.value = msaURL;
}
function do_submit()
{
	add_treedata(); 
	document.forms["mainform"].submit();
}
setTimeout(do_submit, 1000);
</script>
<!--span style='color:red'>Development in progress, functionality may be temporarily impaired</span><p -->
Retrieving phylogenetic tree...<p>

<form name="mainform" method="post" action="$endpoint" onsubmit="add_treedata()">
<!--input type="submit" -->
<input type="hidden" id="json" name="json" value="">
<input type="hidden" id="msa" name="msa" value="">
</form>
END;

		return $text;
	}
	else   # status return other than -1, 0
	{
		$httpcode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
		if ($httpcode == "404")  # hasn't been started
		{
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
			#$text .= "<span style='color:red'>Development in progress, functionality may be temporarily impaired</span><p>\n";
			$text .= "<p>Starting phylogenetic tree computation, please be patient...<p>";
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
						$seqname = str_replace(":",".",$seqname); # TODO
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
					$curName = str_replace(":",".",$curName); #TODO
				}	
				else
				{
					if (isset($user_seqs[$curName]))
					{
						$user_seqs[$curName] .= trim($line);
					}
				}
			}
			#$text .= "<pre>".print_r($user_seqs,TRUE)."</pre>";
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
			#$text .= "<p>$gf_attach<p>";
			$url = "$lorax_url/trees/$fID.$jobkey/sequences";
			#$text .= "<p>Try: $url<p>";
			curl_setopt($ch, CURLOPT_URL, $url);
			$cfile = new CURLFile($gf_attach);
			$post = array("peptide" => $cfile);
			curl_setopt($ch, CURLOPT_POSTFIELDS, $post);
			$ret = curl_exec($ch);
			$httpcode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
			if ($httpcode == 500)
			{
				$text .= "Unable to compute tree for family $fID (no sequences).<p>";
				return $text;
			}
			if ($ret == FALSE || $httpcode != 200)
			{
				$text .= "Sequence upload for tree computation was not successful!<p>";
				tripal_annot_log_error("Failed to upload sequences $gf_attach to lorax server $url (http error $httpcode)");
				return $text;
			}
			$url = "$lorax_url/trees/$fID.$jobkey/hmmalign_FastTree";
			$ch = curl_init();
			curl_setopt($ch, CURLOPT_URL, $url);
			curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1); 
			$ret = curl_exec($ch);
			if ($ret == FALSE || $httpcode != 200)
			{
				$text .= "Launch of tree computation was not successful!<p>";
				tripal_annot_log_error("Failed to start tree computation: $url (http error $httpcode)");
				return $text;
			}
			#$text .= "Launch job: $url<p>ret:$ret<p>";
			$text .= $refresh_text;
			
		}
		else  # a different http status - assume failure
		{
			$text .= "Unable to compute tree (error $httpcode)";
			tripal_annot_log_error("Error from $status_url:$httpcode");
		}
		return $text;

	}
	return $text;

}
function tripal_annot_phylotree_by_name($phylotree_name, &$phylonodes, &$phylonames) {
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
	# the existing gene family nodes. 
	# We use fake "phylonode_ids" for the user nodes and for the 
	# interior nodes. It doesn't seem to cause problems in the display. 

	$stack = array();
	$id_stack = array(); # parallel stack, to track the newly-generated ID's

	$used_ids = array(); 
	foreach ($phylonames as $name => $id)
	{
		$used_ids[$id] = 1;
	}
	
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
			# interior or root node - get a new fake id
			$new_phylonode_id = new_phylonode_id($new_phylonode_id,$used_ids);
			
			if (count($stack) == 0)
			{
				# root node

				$name = $curnode->GetLabel();
				$newnode = array(
					"phylonode_id" => $new_phylonode_id,
					"parent_phylonode_id" => 0, 
					"length" => 0,
		#			"name" => $name,
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
 			$phylonode_id = (int) ($existing ? $phylonode_id : $new_phylonode_id);

			#$text .= "Leaf: $name ID:$phylonode_id  Parent:$cur_parent_id<br>";
			$newnode = array(
 				"phylonode_id" => (int) $phylonode_id,
				"parent_phylonode_id" => (int) $cur_parent_id, 
				"length" => (double) $length,
				"cvterm_name" => "phylo_leaf",
				"name" => $name,
				"feature_id" => ($existing ? $oldnode["feature_id"] : 0),
				"feature_name" => $name,
				"feature_node_id" => ($existing? $oldnode["feature_node_id"] : 0),
				"organism_id" =>  ($existing? $oldnode["organism_id"] : 0),
				"common_name" =>  ($existing? $oldnode["common_name"] : "User Sequence"),
				"abbreviation" =>  ($existing? $oldnode["abbreviation"] : "USeq"),
				"genus" =>  ($existing? $oldnode["genus"] : "*User"),
				"species" =>  ($existing? $oldnode["species"] : "sequences"),
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
function fix_user_seq_names(&$any_data,&$user_seq_names, $jobkey)
{
	$matches = array();
	preg_match_all("/$jobkey\.([^:]+):/", $any_data, $matches);
	#preg_match_all("/($jobkey\.[^:]+):/", $any_data, $matches);
	foreach ($matches[1] as $i => $name)
	{
		$user_seq_names[] = "USR.$name";
	}
	$any_data = str_replace("$jobkey.","USR.",$any_data); 
}
# Test with a specific tree and family
function annot_tree_test()
{
	$fname = "phytozome_10_2.59026816";
	$newick_tree = <<<END
((ambtr.evm_27.model.AmTr_v1.0_scaffold00009.106:0.384627,orysa.LOC_Os09g39790.1:0.497892)aspartic_peptidases:0.0533533,(arath.AT2G42450.1:0.405948,(((vitvi.GSVIVT01035735001:0.184479,vitvi.GSVIVT01028293001:0.0433388):0.167499,(prupe.ppa004711m:0.156533,solly.Solyc06g083920.2.1:0.306974):0.0280413):0.0177991,((araip.Araip.Y7LP9:0.00418805,aradu.Aradu.K38DA:0.00754652):0.182909,(lotja.Lj5g3v1051320.2:0.0907082,((lotja.Lj5g3v1049250.1:0.0900244,(medtr.Medtr1g070195.1:0.0770878,cicar.Ca_07762:0.0457328):0.0287859):0.0261066,(cajca.C.cajan_29010:0.0615653,((vigra.Vradi08g08700.1:0.0432899,phavu.Phvul.007G194900.1:0.0148583):0.0646105,(glyma.Glyma.10G035200.1:0.0221098,glyma.Glyma.13G121500.1:0.0387092):0.0194157):0.0182252):0.0546181):0.0221592):0.040358):0.197562):0.071428):0.0533533):0;
END;

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
		#populate the children[] arrays for each node
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
		$text .= "<pre>".print_r($phylotree_root,TRUE)."</pre>";
return $text;

		$json = json_encode($phylotree_root);
		#$text .= "<pre>$json</pre>";

		$endpoint = "/chado_phylotree/$fname";
		$endpoint .= "?hilite_node=".implode(",",$user_seq_names);

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
function do_submit()
{
	add_treedata(); 
	document.forms["mainform"].submit();
}
setTimeout(do_submit, 1000);
</script>
Retrieving phylogenetic tree...<p>

<form name="mainform" method="post" action="$endpoint" onsubmit="add_treedata()">
<!--input type="submit" -->
<input type="hidden" id="json" name="json" value="">
</form>
END;

		return $text;
}
