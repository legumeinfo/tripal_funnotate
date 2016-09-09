<?php


#################################################################################
#
#	Configuration page
#
################################################################################

function tripal_annot_description_page() 
{
	
	$form_state = array();
	$text = "<hr>";
	$text .= <<< END
For databases, enter the path to the fasta file containing the sequences. 
<br>
The indexed Blast files must be in the same directory, and use the same
filename base. 
<br>
For example, if you have a database
<pre>
/my/database/directory/mydb.fasta
</pre>
then the Blast files must be located in <tt>/my/database/directory</tt> with names
<pre>
mydb.fasta.phr, mydb.fasta.pin, mydb.fasta.psq 
</pre>
<br>
The YAML file should follow the example shown at the bottom of the page.
<br>
Leave empty the two fields <tt>proteins_fasta:, output:</tt> 
<br>
Also leave blank each of the <tt>file:</tt> fields.
<br>
Also make sure that that <tt>database:</tt> fields match up with 
<br>
the blast databases that were entered on this page (order does not matter).
<br>
Use settings appropriate for protein blast output (blastp). They will
<br>
be adjusted automatically for nucleotide (blastx) output as specified in the AHRD manual. 
<hr>
END;
	$text .= drupal_render(drupal_get_form('annot_config_form')); 
	$text .= "<hr>";
	return $text;
}
function annot_config_form($form_state)
{
    $form = array();
	if (NULL == variable_get('annot_upload_sizeMB'))
	{
		variable_set('annot_upload_sizeMB',2);
	}

  $form['annot_blast_path']= array(
    '#type' => 'textfield',
    '#title' => t('Directory of the BLAST+ programs'),
    '#default_value' => variable_get('annot_blast_path', ''),
  );
  $form['annot_java_path']= array(
    '#type' => 'textfield',
    '#title' => t('Path to java executable'),
    '#default_value' => variable_get('annot_java_path', ''),
  );
  $form['annot_ahrd_path']= array(
    '#type' => 'textfield',
    '#title' => t('Path to AHRD jar file'),
    '#default_value' => variable_get('annot_ahrd_path', ''),
  );
  $form['annot_upload_sizeMB']= array(
    '#type' => 'textfield',
    '#title' => t('Max upload size (MB)'),
	'#number_type' => 'integer',
	'#size' => 5,
	'#element_validate' => array('element_validate_integer_positive'), 
    '#default_value' => variable_get('annot_upload_sizeMB', ''),
  );
  $form['annot_db1']= array(
    '#type' => 'textfield',
    '#title' => t('Path to database 1'),
    '#default_value' => variable_get('annot_db1', ''),
  );
  $form['annot_db2']= array(
    '#type' => 'textfield',
    '#title' => t('Path to database 2'),
    '#default_value' => variable_get('annot_db2', ''),
  );
  $form['annot_db3']= array(
    '#type' => 'textfield',
    '#title' => t('Path to database 3'),
    '#default_value' => variable_get('annot_db3', ''),
  );
  $form['annot_db4']= array(
    '#type' => 'textfield',
    '#title' => t('Path to database 4'),
    '#default_value' => variable_get('annot_db4', ''),
  );
  $form['annot_yml']= array(
    '#type' => 'textfield',
    '#title' => t('Path to YAML file for AHRD'),
    '#default_value' => variable_get('annot_yml', ''),
  );

$form['submit'] = array(
    '#type' => 'submit',
    '#value' => 'Save Configuration',
  );


      return $form;
}
function annot_config_form_validate($form, &$form_state) 
{
	$valid = 1;
	$path = $form_state['values']['annot_blast_path'];
	$path .= '/blastp';
	if(!empty($path)) 
	{
		if(file_exists($path) ) 
		{
			variable_set('annot_blast_path', $form_state['values']['annot_blast_path']);
		}
		else 
		{
			$valid = 0;
			form_set_error('annot_blast_path', t('Did not locate blast programs at this path! Please enter a valid path'));
		}
	}
	$path = $form_state['values']['annot_java_path'];
	if(!empty($path)) 
	{
		$last4 = substr($path,-4);
		$last1 = substr($path,-1);
		if ($last4 != "java")
		{
			if ($last1 != "/")
			{
				$path .= "/java";
			}
			else
			{
				$path .= "java";
			}
		}
		if(file_exists($path) ) 
		{
			variable_set('annot_java_path', $path);
		}
		else 
		{
			$valid = 0;
			form_set_error('annot_java_path', t('Did not locate java at this path! Please enter a valid path'));
		}
	}
	$path = $form_state['values']['annot_ahrd_path'];
	if(!empty($path)) 
	{
		$last8 = substr($path,-8);
		$last1 = substr($path,-1);
		if ($last8 != "ahrd.jar")
		{
			if ($last1 != "/")
			{
				$path .= "/ahrd.jar";
			}
			else
			{
				$path .= "ahrd.jar";
			}
		}
		if(file_exists($path) ) 
		{
			variable_set('annot_ahrd_path', $path);
		}
		else 
		{
			$valid = 0;
			form_set_error('annot_ahrd_path', t('Did not locate ahrd.jar at this path! Please enter a valid path'));
		}
	}
	# Check the blast database files
	$numset = 0;
	$blast_dbs = array();
	for ($d = 1; $d <= 4; $d++)
	{
		$var = "annot_db$d";
		$path = $form_state['values'][$var];
		if ($path == "")
		{
			variable_set($var,"");
			continue;
		}
		$blast_dbs[$path] = 1;
		$numset++;
		$exts = array("",".phr",".psq",".pin");
		$ok = 1;
		foreach ($exts as $ext)
		{
			$pathext = $path.$ext;
			if(!file_exists($pathext) ) 
			{
				form_set_error($var, t("Could not find database file $pathext!"));
				$ok = 0;
				break;
			}
		}
		if ($ok == 1)
		{
			variable_set($var,$path);
		}
		else
		{
			$valid = 0;
		}
	}
	if ($numset == 0)
	{
		form_set_error("annot_db1", t("Please enter at least one database"));
		$valid = 0;
	}
	
	# Check the yml file
	$path = $form_state["values"]["annot_yml"];
	if ($path == "")
	{		
		$valid = 0;
		form_set_error($var, t("Please enter a YAML file "));
	}
	$yaml = spyc_load_file($path);

	if (!isset($yaml["proteins_fasta"]))
	{
		$valid = 0;
		form_set_error("annot_yaml", "YAML file is missing proteins_fasta");
	}
	else if ("" != $yaml["proteins_fasta"])
	{
		$valid = 0;
		form_set_error("annot_yaml", "YAML file: proteins_fasta is not empty");
	}
	if (!isset($yaml["output"]))
	{
		$valid = 0;
		form_set_error("annot_yaml", "YAML file is missing output");
	}
	else if ("" != $yaml["output"])
	{
		$valid = 0;
		form_set_error("annot_yaml", "YAML file: output is not empty");
	}
	$yml_dbs = array();
	foreach ($yaml["blast_dbs"] as $dbtag => $dbarr)
	{
		if (!isset($dbarr["file"]))
		{
			$valid = 0;
			form_set_error("annot_yaml", "YAML file is missing 'file' field for $dbtag database");
		}
		else if ("" != $dbarr["file"])
		{
			$valid = 0;
			form_set_error("annot_yaml", "YAML file: 'file' field for $dbtag database is not empty");
		}
		if (!isset($dbarr["database"]))
		{
			$valid = 0;
			form_set_error("annot_yaml", "YAML file is missing 'database' field for $dbtag database");
		}
		else 
		{
			$db = $dbarr["database"];
			if (!file_exists($db))
			{
				$valid = 0;
				form_set_error("annot_yaml", "YAML file: database $db was not found");
			}
			else		
			{
				if (!isset($blast_dbs[$db]))
				{
					$valid = 0;
					form_set_error("annot_yaml", "YAML file contains database $db which was not entered as a blast database");
				}
				$yml_dbs[$db] = 1; 
			}
		}
	}
	foreach ($blast_dbs as $db => $val)
	{
				if (!isset($yml_dbs[$db]))
				{
					$valid = 0;
					form_set_error("annot_yaml", "Blast database $db is not used in the YAML file");
				}
		
	}
	variable_set("annot_yml",$path);

	$maxsize = $form_state['values']['annot_upload_sizeMB'];
	$sysmax = (file_upload_max_size()/1024)/1024;
	if ($maxsize > $sysmax)
	{
		form_set_error('annot_upload_sizeMB',t("upload size is larger than server maximum $sysmax MB"));
	}
	if ($valid == 0)
	{
		drupal_set_message("The configuration failed!","error");
	}
}
function config_form_submit($form, &$form_state)
{
	drupal_set_message('The form has been submitted.');
}
