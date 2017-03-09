<?php
#
# Newick parse code borrowed from R. Page, github: rdmpage/deep-tree
#
define('CHILD', 0);
define ('SIB', 1);
//--------------------------------------------------------------------------------------------------
function write_nexus_label($label)
{
	if (preg_match('/^(\w|\d)+$/', $label))
	{
	}
	else
	{
		str_replace ("'", "\'", $label);
		$label = "'" . $label . "'";
	}
	return $label;
}
//--------------------------------------------------------------------------------------------------
/**
 * @brief Node in a tree
 *
 * Node has pointers to child, sibling, and ancestral node, these pointers are
 * NULL if corresponding node doesn't exist. Has label as a field, all other values
 * are stored in an key-value array of attributes.
 */
class tripal_annot_Node
{
	var $ancestor;
	var $child;
	var $sibling;
	var $label;
	var $id;
	var $attributes = array();
	var $cluster = array();
	
	//----------------------------------------------------------------------------------------------
	function __construct($label = '')
	{
		$this->ancestor = NULL;
		$this->child = NULL;
		$this->sibling = NULL;
		$this->label = $label;
		$this->cluster = array();
	}
		
	//----------------------------------------------------------------------------------------------
	function IsLeaf()
	{
		return ($this->child == NULL);
	}
	
	//----------------------------------------------------------------------------------------------
	function AddWeight($w)
	{
		$w0 = $this->GetAttribute('weight');
		$this->SetAttribute('weight', $w0 + $w);
	}
	
	//----------------------------------------------------------------------------------------------
	function Dump()
	{
		echo "---Dump Node---\n";
		echo "   Label: " . $this->label . "\n";
		echo "      Id: " . $this->id . "\n";
		echo "   Child: ";
		if ($this->child == NULL)
		{
			echo "NULL\n";
		}
		else
		{
			echo $this->child->label . "\n";
		}
		echo " Sibling: ";
		if ($this->sibling == NULL)
		{
			echo "NULL\n";
		}
		else
		{
			echo $this->sibling->label . "\n";
		}
		echo "Ancestor: ";
		if ($this->ancestor == NULL)
		{
			echo "NULL\n";
		}
		else
		{
			echo $this->ancestor->label . "\n";
		}
		echo "Attributes:\n";
		print_r($this->attributes);
		echo "Cluster:\n";
		print_r($this->cluster);
	}
	
	//----------------------------------------------------------------------------------------------
	function GetAncestor() { return $this->ancestor; }	
	
	//----------------------------------------------------------------------------------------------
	function GetAttribute($key) 
	{
		if (isset($this->attributes[$key]))
		{			
			return $this->attributes[$key];
		}
		else
		{
			return null;
		}
	}		
	//----------------------------------------------------------------------------------------------
	function GetChild() { return $this->child; }	
	//----------------------------------------------------------------------------------------------
	function GetId() { return $this->id; }	
	//----------------------------------------------------------------------------------------------
	function GetLabel() { return $this->label; }	
	
	//----------------------------------------------------------------------------------------------
	// If node is sibling get node immediately preceding it ("to the left")
	function GetLeftSibling()
	{
		$q = $this->ancestor->child;
		while ($q->sibling != $this)
		{
			$q = $q->sibling;
		}
		return $q;
	}
	
	//----------------------------------------------------------------------------------------------
	function GetRightMostSibling()
	{
		$p = $this;
		
		while ($p->sibling)
		{
			$p = $p->sibling;
		}
		return $p;
	}
	//----------------------------------------------------------------------------------------------
	function GetSibling() { return $this->sibling; }	
	
	//----------------------------------------------------------------------------------------------
	function IsChild()
	{
		$is_child = false;
		$q = $this->ancestor;
		if ($q)
		{
			$is_child = ($q->child == $this);
		}
		return $is_child;
	}
	
	//----------------------------------------------------------------------------------------------
	function SetAncestor($p)
	{
		$this->ancestor = $p;
	}
	
	//----------------------------------------------------------------------------------------------
	function SetAttribute($key, $value)
	{
		$this->attributes[$key] = $value;
	}
	
	//----------------------------------------------------------------------------------------------
	function SetChild($p)
	{
		$this->child = $p;
	}
	
	//----------------------------------------------------------------------------------------------
	function SetId($id)
	{
		$this->id = $id;
	}
	
	//----------------------------------------------------------------------------------------------
	function SetLabel($label)
	{
		$this->label = $label;
	}
	
	//----------------------------------------------------------------------------------------------
	function SetSibling($p)
	{
		$this->sibling = $p;
	}
	
	//----------------------------------------------------------------------------------------------
	// Children of node (as array)
	function GetChildren()
	{
		$children = array();
		$p = $this->child;
		if ($p)
		{
			array_push($children, $p);
			$p = $p->sibling;
			while ($p)
			{
				array_push($children, $p);
				$p = $p->sibling;
			}
		}
		return $children;
	}
	
	
}
//--------------------------------------------------------------------------------------------------
class tripal_annot_Tree
{
	var $root;
	var $num_nodes;
	var $label_to_node_map = array();
	var $nodes = array();	
	var $num_leaves;
	var $rooted = true;
	var $has_edge_lengths = false;
	//----------------------------------------------------------------------------------------------
	function __construct()
	{
		$this->root = NULL;;
		$this->num_nodes = 0;
		$this->num_leaves = 0;
	}
	
	//----------------------------------------------------------------------------------------------
	function GetNumLeaves() { return $this->num_leaves; }
	
	
	//----------------------------------------------------------------------------------------------
	function GetRoot() { return $this->root; }
//----------------------------------------------------------------------------------------------
	function HasBranchLengths() { return $this->has_edge_lengths; }
	//----------------------------------------------------------------------------------------------
	function IsRooted() { return $this->rooted; }
	
	//----------------------------------------------------------------------------------------------
	function SetRoot($root)
	{
		$this->root = $root;
	}
	
	//----------------------------------------------------------------------------------------------
	function NodeWithLabel($label)
	{
		$p = NULL;
		if (in_array($label, $this->label_to_node_map))
		{
			$p = $this->nodes[$this->label_to_node_map[$label]];
		}
		return $p;
	}
	
	//----------------------------------------------------------------------------------------------
	function NewNode($label = '')
	{
		$node = new tripal_annot_Node($label);
		$node->id = $this->num_nodes++;
		$this->nodes[$node->id] = $node;
		if ($label != '')
		{
			$this->label_to_node_map[$label] = $node->id;
		}
		else
		{
			/*$label = "_" . $node->id;
			$node->SetLabel($label);
			$this->label_to_node_map[$label] = $node->id;*/
		}		
		return $node;
	}
	
	//----------------------------------------------------------------------------------------------
	function Parse ($str)
	{
		$str = str_replace('\\', "", $str);
		
		$str = str_replace("(", "|(|", $str);
		$str = str_replace(")", "|)|", $str);
		$str = str_replace(",", "|,|", $str);
		$str = str_replace(":", "|:|", $str);
		$str = str_replace(";", "|;|", $str);
		$str = str_replace("||", "|", $str);
		
		$token = explode("|", $str);
		
		//print_r($token);
		
		$curnode = $this->NewNode();
		$this->root = $curnode;
		$state = 0;
		$stack = array();
		$n = count($tokens);
		
		$i = 1;
		while ($state != 99)
		{
			switch ($state)
			{
				case 0: // getname
					if (ctype_alnum($token[$i]{0}))
					{
						$this->num_leaves++;
						
						$label = $token[$i];
						
	
						// kml
						if (preg_match('/^(?<label>.*)\s*lat=(?<lat>.*)long=(?<long>.*)$/Uu', $label, $m))
						{
							$curnode->SetAttribute('lat', $m['lat']);
							$curnode->SetAttribute('long', $m['long']);
							$label = $m['label'];
						}
						
						$curnode->SetLabel($label);
						
						
						$i++;
						$state = 1;
					}
					else 
					{
						if ($token[$i]{0} == "'")
						{
							$label = $token[$i];
							$label = preg_replace("/^'/", "", $label);
							$label = preg_replace("/'$/", "", $label);
							$this->num_leaves++;
							
							
							// kml
							if (preg_match('/^(?<label>.*)\s*lat=(?<lat>.*)long=(?<long>.*)$/Uu', $label, $m))
							{
								$curnode->SetAttribute('lat', $m['lat']);
								$curnode->SetAttribute('long', $m['long']);
								$label = $m['label'];
							}
							
							$curnode->SetLabel($label);
							
							
							
							$i++;
							$state = 1;
							
						}
						else
						{
							switch ($token[$i])
							{
								case '(':
									$state = 2;
									break;
								default:
									$state = 99;
									break;
							}
						}
					}
					break;
					
				case 1: // getinternode
					switch ($token[$i])
					{
						case ':':
						case ',':
						case ')':
							$state = 2;
							break;
						default:
							$state = 99;
							break;
					}
					break;
					
				case 2: // nextmove
					switch ($token[$i])
					{
						case ':':
							$i++;
							if (is_numeric($token[$i]))
							{
								$curnode->SetAttribute('edge_length', $token[$i]);
								$this->has_edge_lengths = true;
								$i++;
							}
							break;
						case ',':
							$q = $this->NewNode();
							$curnode->SetSibling($q);
							$c = count($stack);
							if ($c == 0)
							{
								$state = 99;
							}
							else
							{
								$q->SetAncestor($stack[$c - 1]);
								$curnode = $q;
								$state = 0;
								$i++;
							}
							break;							
						case '(':
							$stack[] = $curnode;
							$q = $this->NewNode();
							$curnode->SetChild($q);
							$q->SetAncestor($curnode);
							$curnode = $q;
							$state = 0;
							$i++;
							break;
						case ')':
							if (empty($stack))
							{
								$state = 99;
							}
							else
							{
								$curnode = array_pop($stack);
								$state = 3;
								$i++;
							}
							/*
							$c = count($stack);
							if ($c == 0)
							{
								$state = 99;
							}
							else
							{
								$q = $stack[$c - 1];
								$curnode = $q;
								array_pop($stack);
								$state = 3;
								$i++;
							}*/
							break;
						
						case ';':
							if (empty($stack))
							{
								$state = 99;
							}
							else
							{
								$state = 99;
							}
							/*
							$c = count($stack);
							if ($c == 0)
							{
								$state = 99;
							}
							else
							{
								$state = 99;
							} */
							break;
						
						default:
							$state = 99;
							break;
					}
					break;
				
				case 3: // finishchildren
					if (ctype_alnum($token[$i]{0}))
					{
						$curnode->SetLabel($token[$i]);
						$i++;
					}
					else
					{
						switch ($token[$i])
						{
							case ':':
								$i++;
								if (is_numeric($token[$i]))
								{
									$curnode->SetAttribute('edge_length', $token[$i]);
									$this->has_edge_lengths = true;
									$i++;
								}
								break;
							case ')':
								$c = count($stack);
								if ($c == 0)
								{
									$state = 99;
								}
								else
								{
									$q = $stack[$c - 1];
									$curnode = $q;
									array_pop($stack);
									$i++;
								}
								break;
							case ',':
								$q = $this->NewNode();
								$curnode->SetSibling($q);
								$c = count($stack);
								if ($c == 0)
								{
									$state = 99;
								}
								else
								{
									$q->SetAncestor($stack[$c - 1]);
									$curnode = $q;
									$state = 0;
									$i++;
								}
								break;
							case ';':
								$state = 2;
								break;
							default:
								$state = 99;
								break;
						}
					}
					break;
			}
		}
						
	}		
						
		
	//----------------------------------------------------------------------------------------------
	function WriteNewick()
	{
		$newick = '';
		
		$stack = array();
		$curnode = $this->root;
		
		while ($curnode != NULL)
		{	
			if ($curnode->GetChild())
			{
				$newick .= '(';
				$stack[] = $curnode;
				$curnode = $curnode->GetChild();
			}
			else
			{
				$newick .= write_nexus_label($curnode->GetLabel());
				
				$length = $curnode->GetAttribute('edge_length');
				if ($length != '')
				{
					$newick .= ':' . $length;
				}
											
				while (!empty($stack) && ($curnode->GetSibling() == NULL))
				{
					$newick .= ')';
					$curnode = array_pop($stack);
					
					// Write internal node
					if ($curnode->GetLabel() != '')
					{
						$newick .= write_nexus_label($curnode->GetLabel());
					}
					$length = $curnode->GetAttribute('edge_length');
					if ($length != '')
					{
						$newick .= ':' . $length;
					}					
				}
				if (empty($stack))
				{
					$curnode = NULL;
				}
				else
				{
					$newick .= ',';
					$curnode = $curnode->GetSibling();
				}
			}		
		}
		$newick .= ";";
		return $newick;
	}	
			
	
	//----------------------------------------------------------------------------------------------
	// Build weights
	function BuildWeights($p)
	{
		if ($p)
		{
			$p->SetAttribute('weight', 0);
			
			$this->BuildWeights($p->GetChild());
			$this->BuildWeights($p->GetSibling());
			
			if ($p->Isleaf())
			{
				$p->SetAttribute('weight', 1);
			}
			if ($p->GetAncestor())
			{
				$p->GetAncestor()->AddWeight($p->GetAttribute('weight'));
			}
		}
	}
}
?>

