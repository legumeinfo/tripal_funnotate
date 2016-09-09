# tripal_funnotate

A Tripal/Drupal module enabling functional annotation of user-uploaded data.

Requirements:  
Tripal
Java  
Blast+  
AHRD [https://github.com/groupschoof/AHRD]  
Spyc [https://github.com/mustangostang/spyc]  
One or more Blast-formatted protein databases (e.g., swissprot)  
A Yaml file for AHRD (further instructions are on the module config page)  

Installation:

Copy the tripal_annot directory to the appropriate location in   
your Drupal tree, e.g.: `/usr/local/www/drupal7/sites/all/modules/`

Copy Spyc.php to the `includes` directory

Add the module through Drupal (`/admin/modules`)  
The module creates one database table `tripal_annot`

Configure the module as directed on the config page  

Operation:

The module uploads a user file (nucleotide or protein), blasts it against the  
configured databases, and runs AHRD to parse the results.  
The AHRD output and all blast outputs are provided for download.  

The main page will be in the Drupal tree at `/annot`.    
This page uploads the query file and redirects to landing page `/annot/upload/NNN`  
where `NNN` is a sequential number (`fid` from the Drupal `file_managed` table).  
The landing page shows results of sanity-checking the file.  
If the file is satisfactory, user then (optionally) enters email and launches the job.  
The job is assigned a random 4-letter tag (e.g., `FHGK`) and    
user redirects to the job tracking page `/annot/job/FHGK`.  
This page tracks job progress and provides download links on completion.  
If email was supplied, notifications are sent at launch and completion.  
