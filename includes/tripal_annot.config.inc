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
	# TODO delete these testing lines
	#$linkpath = tripal_get_files_dir()."/annot_link";
	#$linktarg = "/tmp/annot_files";
	#system("chmod 755 $linktarg");
	#system("rm -Rf $linktarg");
	#mkdir($linktarg,755);
	#system("ln -s $linktarg $linkpath");
	#system("chmod 755 $linkpath");
	#rmdir($linktarg);
	#$annotdir = DRUPAL_ROOT."/".tripal_get_files_dir()."/annot";
	#system("chmod 755 $annotdir");
	#system("rm -R $annotdir");

    $form = array();
	if (NULL == variable_get('annot_upload_sizeKB'))
	{
		variable_set('annot_upload_sizeKB',100);
	}
	if (NULL == variable_get('annot_use_email'))
	{
		variable_set('annot_use_email',0);
	}
	if (NULL == variable_get('annot_files_dir'))
	{
		variable_set('annot_files_dir',"annot");
	}
	$files_topdir = tripal_get_files_dir();

	$form['annot_upload_sizeKB']= array(
		'#type' => 'textfield',
		'#title' => t('Max upload size (KB)'),
		'#number_type' => 'integer',
		'#size' => 5,
		'#element_validate' => array('element_validate_integer_positive'), 
		'#default_value' => variable_get('annot_upload_sizeKB', ''),
	);
	$form['annot_use_email']= array(
		'#type' => 'radios',
		'#title' => t('Enable email notification'),
		'#options' => array(1 => t("yes"), 0 => t("no")),
		'#default_value' => variable_get('annot_use_email', 0)
	);
	$form['annot_files_dir']= array(
		'#type' => 'textfield',
		'#title' => t('Directory (or link) name for output files under '.$files_topdir),
		'#default_value' => variable_get('annot_files_dir', ''),
	);
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
	$form['annot_iprdir']= array(
		'#type' => 'textfield',
		'#title' => t('Path to InterProScan directory'),
		'#default_value' => variable_get('annot_iprdir', ''),
	);
	$form['annot_iprcfgdir']= array(
		'#type' => 'textfield',
		'#title' => t('Directory for the InterProScan properties file'),
		'#default_value' => variable_get('annot_iprcfgdir', ''),
	);
	$form['annot_iprparam']= array(
		'#type' => 'textfield',
		'#title' => t('InterProScan parameters (optional)'),
		'#default_value' => variable_get('annot_iprparam', ''),
	);
	$form['annot_hmmdir']= array(
		'#type' => 'textfield',
		'#title' => t('Path to HMMER top directory (in which the binaries directory is found)'),
		'#default_value' => variable_get('annot_hmmdir', ''),
	);
	$form['annot_hmmdb']= array(
		'#type' => 'textfield',
		'#title' => t('Path to HMMER database (hmm file)'),
		'#default_value' => variable_get('annot_hmmdb', ''),
	);
	$form['annot_hmmparam']= array(
		'#type' => 'textfield',
		'#title' => t('HMMER parameters (optional)'),
		'#default_value' => variable_get('annot_hmmparam', ''),
	);
	$form['annot_escanpath']= array(
		'#type' => 'textfield',
		'#title' => t('Path to ESTScan executable'),
		'#default_value' => variable_get('annot_escanpath', ''),
	);
	$form['annot_smat']= array(
		'#type' => 'textfield',
		'#title' => t('Path to ESTScan matrix (smat file)'),
		'#default_value' => variable_get('annot_smat', ''),
  	);
	$form['annot_escanparam']= array(
		'#type' => 'textfield',
		'#title' => t('ESTScan parameters (optional)'),
		'#default_value' => variable_get('annot_escanparam', ''),
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
	$dir = $form_state['values']['annot_files_dir'];
	if(!empty($dir)) 
	{
		if (preg_match("/^\w+$/",$dir))
		{
			$fulldir = DRUPAL_ROOT."/".tripal_get_files_dir()."/$dir";
			if (is_dir($fulldir))
			{
				variable_set('annot_files_dir', $form_state['values']['annot_files_dir']);
			}
			else
			{
				# Check whether this already exists as a broken link that we need to fix
				if (is_link($fulldir))
				{
					$realpath = readlink($fulldir);
					mkdir($realpath,755);
					system("chmod 755 $realpath");
					if (is_dir($fulldir))
					{
						variable_set('annot_files_dir', $form_state['values']['annot_files_dir']);
					}
					else
					{
						$valid = 0;
						form_set_error('annot_files_dir', t("Files directory $fulldir is a broken link which could not be fixed"));
					}
				}
				else
				{
					mkdir($fulldir,755);
					system("chmod 755 $fulldir");
					if (is_dir($fulldir))
					{
						variable_set('annot_files_dir', $form_state['values']['annot_files_dir']);
					}
					else
					{
						$valid = 0;
						form_set_error('annot_files_dir', t("Files directory $fulldir could not be created"));
					}
				}
			}
		}
		else 
		{
			$valid = 0;
			form_set_error('annot_files_dir', t('Files directory can contain only letters, numbers and underscores'));
		}
	}
	else
	{
		$valid = 0;
		form_set_error('annot_files_dir', t('Please enter a valid files directory name'));
	}
	$path = $form_state['values']['annot_blast_path'];
	if(!empty($path)) 
	{
		$path .= '/blastp';
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
	else
	{
		$valid = 0;
		form_set_error('annot_blast_path', t('Please enter a valid path for Blast+'));
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
	else
	{
		$valid = 0;
		form_set_error('annot_java_path', t('Please enter a valid path for Java'));
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
	else
	{
		$valid = 0;
		form_set_error('annot_ahrd_path', t('Please enter a valid path for AHRD'));
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

	$path = $form_state['values']['annot_iprdir'];
	if(!empty($path)) 
	{
		$path .= '/interproscan.sh';
		if(file_exists($path) ) 
		{
			variable_set('annot_iprdir', $form_state['values']['annot_iprdir']);
		}
		else 
		{
			$valid = 0;
			form_set_error('annot_iprdir', t('Did not locate interproscan.sh at this path! Please enter a valid path'));
		}
	}
	else
	{
		$valid = 0;
		form_set_error('annot_iprdir', t('Please enter a valid path for InterProScan'));
	}

	$path = $form_state['values']['annot_iprcfgdir'];
	if(!empty($path)) 
	{
		if(is_dir($path) ) 
		{
			system("touch $path/tripalannottest");
			if (file_exists("$path/tripalannottest"))
			{
				system("rm $path/tripalannottest");
				variable_set('annot_iprcfgdir', $form_state['values']['annot_iprcfgdir']);
			}
			else
			{
				$valid = 0;
				form_set_error('annot_iprcfgdir', t('Could not write to directory '.$path.' Please enter a writeable properties directory'));

			}
		}
		else 
		{
			$valid = 0;
			form_set_error('annot_iprcfgdir', t('Could not find directory '.$path.' Please enter a valid properties directory'));
		}
	}
	else
	{
		$valid = 0;
		form_set_error('annot_iprcfgdir', t('Please enter a valid properties directory'));
	}
	$params = $form_state['values']['annot_iprparam'];
	if(!empty($params)) 
	{
		variable_set('annot_iprparam', $form_state['values']['annot_iprparam']);
	}
	$path = $form_state['values']['annot_hmmdir'];
	if(!empty($path) && is_dir($path)) 
	{
		$path .= '/binaries/hmmsearch';
		if(file_exists($path) ) 
		{
			variable_set('annot_hmmdir', $form_state['values']['annot_hmmdir']);
		}
		else 
		{
			$valid = 0;
			form_set_error('annot_hmmdir', t('Did not locate binaries/hmmsearch in this directory! Please enter a valid directory'));
		}
	}
	else
	{
		$valid = 0;
		form_set_error('annot_hmmdir', t('Please enter a valid directory for HMMER'));
	}
	$path = $form_state['values']['annot_hmmdb'];
	if(!empty($path) && is_file($path)) 
	{
		$exts = array("h3f","h3i","h3m","h3p");
		foreach ($exts as $ext)
		{
			if(!file_exists("$path.$ext") ) 
			{
				$valid = 0;
				form_set_error('annot_hmmdb', t('Did not locate the ').".$ext".t(' file with the .hmm file!'));
			}
		}
		if ($valid)
		{
			variable_set('annot_hmmdb', $form_state['values']['annot_hmmdb']);
		}
	}
	else
	{
		$valid = 0;
		form_set_error('annot_hmmdb', t('Please enter a valid path to the HMMER database hmm file'));
	}
	$path = $form_state['values']['annot_escanpath'];
	if(!empty($path)) 
	{
		if (is_dir($path))
		{
			$path .= '/estscan';
		}
		if(file_exists($path) ) 
		{
			variable_set('annot_escanpath', $path);
		}
		else 
		{
			$valid = 0;
			form_set_error('annot_escanpath', t('$path does not exist! Please enter a valid path for estscan'));
		}
	}
	else
	{
		$valid = 0;
		form_set_error('annot_escanpath', t('Please enter a valid path for estscan'));
	}
	$path = $form_state['values']['annot_smat'];
	if(!empty($path)) 
	{
		if(file_exists($path) ) 
		{
			variable_set('annot_smat', $form_state['values']['annot_smat']);
		}
		else 
		{
			$valid = 0;
			form_set_error('annot_smat', t('$path does not exist! Please enter a valid .smat file'));
		}
	}
	else
	{
		$valid = 0;
		form_set_error('annot_smat', t('Please enter a valid .smat file'));
	}
	$params = $form_state['values']['annot_escanparam'];
	if(!empty($params)) 
	{
		variable_set('annot_escanparam', $form_state['values']['annot_escanparam']);
	}

	$maxsize = $form_state['values']['annot_upload_sizeKB'];
	$sysmax = (file_upload_max_size()/1024);
	if ($maxsize > $sysmax)
	{
		form_set_error('annot_upload_sizeKB',t("upload size is larger than server maximum $sysmax KB"));
		$valid = 0;
	}
	else
	{
		variable_set('annot_upload_sizeKB', $form_state['values']['annot_upload_sizeKB']);
	}
	variable_set('annot_use_email', $form_state['values']['annot_use_email']);
	if ($valid == 0)
	{
		drupal_set_message("The configuration failed!","error");
	}
}
function config_form_submit($form, &$form_state)
{
	drupal_set_message('The form has been submitted.');
}
