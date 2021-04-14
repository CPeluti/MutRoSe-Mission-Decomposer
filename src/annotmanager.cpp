#include "annotmanager.hpp"

#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <regex>
#include <sstream>
#include <set>

#include "rannot.hpp"

using namespace std;

const string world_db_query_var = "location_db";

int parse_string(const char* in);

map<string,general_annot*> goals_and_rannots;

/*
    Function: retrieve_runtime_annot
    Objective: Retrieve the runtime annotation of some given node. We need to have goals_and_rannots initialized

    @ Input: The text of the desired node in the goal model
    @ Output: The runtime annotation of the given node
*/ 
general_annot* retrieve_runtime_annot(string id) {
    parse_string(id.c_str());

    string node_name = get_node_name(id);

    return goals_and_rannots[node_name];
}

/*
    Function: retrieve_gm_annot
    Objective: Retrieve the runtime annotation of the whole goal model

    @ Input 1: The goal model as a GMGraph object
    @ Input 2: The ptree object representing the world database
    @ Input 3: The high-level location type
    @ Input 4: The abstract task instances vector
    @ Output: The goal model runtime annotation
*/ 
general_annot* retrieve_gm_annot(GMGraph gm, pt::ptree worlddb, vector<string> high_level_loc_types, map<string,vector<AbstractTask>> at_instances) {
    vector<int> vctr = get_dfs_gm_nodes(gm);
    
    VertexData root = gm[vctr.at(0)];

    general_annot* root_annot = retrieve_runtime_annot(root.text);

    recursive_fill_up_runtime_annot(root_annot, gm[vctr.at(0)]);

    map<string,pair<string,vector<pt::ptree>>> valid_variables;

    map<int,int> node_depths;
    map<int,AchieveCondition> valid_forAll_conditions;

    node_depths[vctr.at(0)] = 0;

    int current_node = vctr.at(0);

    vctr.erase(vctr.begin());

    general_annot* empty_annot = new general_annot();
    empty_annot->content = "";
    empty_annot->related_goal = "";

    root_annot->parent = empty_annot;

    recursive_gm_annot_generation(root_annot, vctr, gm, worlddb, high_level_loc_types, current_node, valid_variables, valid_forAll_conditions, node_depths);

    return root_annot;
}

/*
    Function: recursive_gm_annot_generation
    Objective: Recusive generation of the goal model runtime annotation

    @ Input 1: The current node runtime annotation, generated for the whole goal model so far
    @ Input 2: The nodes indexes visited in a depth-first search manner
    @ Input 3: The goal model as a GMGraph object
    @ Input 4: The world knowledge ptree object
    @ Input 5: The high-level location type
    @ Input 6: The current node index
    @ Input 7: The valid variables, given the query goals select statements
    @ Input 8: The valid forAll conditions, given the achieve goals
    @ Input 9: The map of node depths
    @ Output: Void. The runtime goal model annotation is generated
*/ 
void recursive_gm_annot_generation(general_annot* node_annot, vector<int>& vctr, GMGraph gm, pt::ptree worlddb, vector<string> high_level_loc_types, int current_node,
                                        map<string,pair<string,vector<pt::ptree>>>& valid_variables, map<int,AchieveCondition> valid_forAll_conditions, 
                                        map<int,int>& node_depths) {
    set<string> operators {";","#","FALLBACK","OPT","|"};

    set<string>::iterator op_it;

    op_it = operators.find(node_annot->content);

    int depth;
    if(gm[current_node].parent != -1) {
        depth = node_depths[gm[current_node].parent] + 1;
        node_depths[current_node] = depth;
    } else {
        depth = node_depths[current_node];
    }

    bool is_forAll_goal = false;
    if(gm[current_node].type == "istar.Goal") {
		if(std::get<string>(gm[current_node].custom_props["GoalType"]) == "Query") {
			/*vector<pt::ptree> aux;
			QueriedProperty q = std::get<QueriedProperty>(gm[current_node].custom_props["QueriedProperty"]);
			BOOST_FOREACH(pt::ptree::value_type& child, worlddb.get_child("world_db")) {
				if(child.first == q.query_var.second) { //If type of queried var equals type of the variable in the database, check condition (if any)
					if(q.query.size() == 1) {
						if(q.query.at(0) != "") {
							string prop = q.query.at(0).substr(q.query.at(0).find('.')+1);
							bool prop_val;
							istringstream(boost::to_lower_copy(child.second.get<string>(prop))) >> std::boolalpha >> prop_val;
							if(q.query.at(0).find('!') != string::npos) {
								prop_val = !prop_val;
							}
							if(prop_val) aux.push_back(child.second);
						} else {
							aux.push_back(child.second);
						}
					} else {
						string prop = q.query.at(0).substr(q.query.at(0).find('.')+1);
						string prop_val = child.second.get<string>(prop);
						bool result;
						if(q.query.at(1) == "==") {
							result = (prop_val == q.query.at(2));
						} else {
							result = (prop_val != q.query.at(2));
						}
						if(result) aux.push_back(child.second);
					}
				}
			}
			string var_name = std::get<vector<pair<string,string>>>(gm[current_node].custom_props["Controls"]).at(0).first; //.second would be for type assertion
			valid_variables[var_name] = make_pair(q.query_var.second,aux);*/

            QueriedProperty q = std::get<QueriedProperty>(gm[current_node].custom_props["QueriedProperty"]);
            solve_query_statement(worlddb.get_child("world_db"),q,gm,current_node,valid_variables);
		} else if(std::get<string>(gm[current_node].custom_props["GoalType"]) == "Achieve") {
            is_forAll_goal = true;
			AchieveCondition a = std::get<AchieveCondition>(gm[current_node].custom_props["AchieveCondition"]);
			if(a.has_forAll_expr) {
				valid_forAll_conditions[depth] = a;
			}
		}
    }

    /*
        -> If we have an operator, simply check its children.

        -> If we have a Goal/Task we have two cases:
            - We may be dealing with a leaf node, in which case we simply finish the execution or
            - We may be dealing with a non-leaf node, in which case we expand it and substitute it for its extension in the parent's children
    */
    if(op_it != operators.end()) { //GM root goal    
        for(general_annot* child : node_annot->children) {
            int c_node = vctr.at(0);
            child->parent = node_annot;
            recursive_gm_annot_generation(child, vctr, gm, worlddb, high_level_loc_types, c_node, valid_variables, valid_forAll_conditions, node_depths);
        }

        if(is_forAll_goal) {
            string iterated_var = valid_forAll_conditions[depth].get_iterated_var();
            int generated_instances = valid_variables[iterated_var].second.size();

            /*
                Generation of multiple instances of the forAll goal and its children
            */
            if(generated_instances > 1) {
                general_annot* aux = new general_annot();

                aux->content = node_annot->content;
                aux->type = node_annot->type;
                aux->children = node_annot->children;
                aux->related_goal = node_annot->related_goal;
                
                node_annot->content = "#";
                node_annot->type = OPERATOR;
                node_annot->children.clear();
                node_annot->related_goal = "";
                for(int i = 0;i < generated_instances;i++) {
                    general_annot* child = new general_annot();
                    
                    child->content = aux->content;
                    child->type = aux->type;
                    child->children = aux->children;
                    child->related_goal = aux->related_goal;

                    node_annot->children.push_back(child);
                }
            }
        }
    } else {
        recursive_fill_up_runtime_annot(node_annot, gm[vctr.at(0)]);

        if(gm[vctr.at(0)].children.size() == 0) { //Leaf Node
            vctr.erase(vctr.begin());
            return;
        } else {
            general_annot* expanded_annot = retrieve_runtime_annot(gm[vctr.at(0)].text);

            if(gm[vctr.at(0)].children.size() > 1) {
                if(expanded_annot->content == "") {
                    expanded_annot->content = ";";
                    for(int child : gm[vctr.at(0)].children) {
                        general_annot* aux = new general_annot();

                        string node_name = get_node_name(gm[child].text);

                        aux->content = node_name;
                        if(node_name.front() == 'G') {
                            aux->type = GOAL;
                        } else {
                            aux->type = TASK;
                        }

                        expanded_annot->children.push_back(aux);
                    }
                }
            } else { //Means-end decomposition
                int only_child = gm[vctr.at(0)].children.at(0);
                expanded_annot->content = get_node_name(gm[vctr.at(0)].text);
                expanded_annot->type = MEANSEND;

                general_annot* aux = new general_annot();

                string node_name = get_node_name(gm[only_child].text);
                
                aux->content = node_name;
                if(node_name.front() == 'G') {
                    aux->type = GOAL;
                } else {
                    aux->type = TASK;
                }

                if(aux->type == TASK) {
                    pair<string,string> node_name_id = parse_at_text(gm[only_child].text);
                    aux->content = node_name_id.first;
                }

                expanded_annot->children.push_back(aux);
            }

            node_annot->content = expanded_annot->content;
            node_annot->type = expanded_annot->type;
            node_annot->children = expanded_annot->children;
            node_annot->related_goal = expanded_annot->related_goal;
            
            vctr.erase(vctr.begin());
            for(general_annot* child : node_annot->children) {
                int c_node = vctr.at(0);
                child->parent = node_annot;
                recursive_gm_annot_generation(child, vctr, gm, worlddb, high_level_loc_types, c_node, valid_variables, valid_forAll_conditions, node_depths);
            }

            std::cout << "is forAll goal? " << is_forAll_goal << std::endl;
            std::cout << "Goal Name: " << get_node_name(gm[current_node].text) << std::endl;
            if(is_forAll_goal) {
                string iterated_var = valid_forAll_conditions[depth].get_iterated_var();
                int generated_instances = valid_variables[iterated_var].second.size();

                std::cout << "iterated var: " << iterated_var << std::endl;
                std::cout << "valid_variables[iterated_var].second.size(): " << valid_variables[iterated_var].second.size() << std::endl;

                /*
                    Generation of multiple instances of the forAll goal and its children
                */
                if(generated_instances > 1) {
                    std::cout << "Generated instances > 1" << std::endl;
                    general_annot* aux = new general_annot();

                    aux->content = node_annot->content;
                    aux->type = node_annot->type;
                    aux->children = node_annot->children;
                    aux->related_goal = node_annot->related_goal;
                    aux->non_coop = node_annot->non_coop;
                    aux->group = node_annot->group;
                    aux->divisible = node_annot->divisible;

                    node_annot->content = "#"; //Do we need to define a custom operator for instances generated by a forAll expression?
                    node_annot->type = OPERATOR;
                    node_annot->children.clear();
                    node_annot->related_goal = "";
                    node_annot->group = true;
                    node_annot->divisible = true;
                    for(int i = 0;i < generated_instances;i++) {
                        general_annot* child = new general_annot();
                        
                        child->content = aux->content;
                        child->type = aux->type;
                        child->non_coop = aux->non_coop;
                        child->group = aux->group;
                        child->divisible = aux->divisible;
                        for(general_annot* ch : aux->children) {
                            general_annot* copy = new general_annot();

                            copy->content = ch->content;
                            copy->type = ch->type;
                            copy->related_goal = ch->related_goal;
                            copy->non_coop = ch->non_coop;
                            copy->group = ch->group;
                            copy->divisible = ch->divisible;
                            recursive_child_replacement(copy, ch);
                            
                            copy->parent = child;
                            child->children.push_back(copy);
                        }
                        child->related_goal = aux->related_goal;

                        child->parent = node_annot;
                        node_annot->children.push_back(child);
                    }
                }
            }
        }
    }
}

void recursive_fill_up_runtime_annot(general_annot* rannot, VertexData gm_node) {
    if(!gm_node.group || (gm_node.group && !gm_node.divisible)) {
        rannot->non_coop = true;
    } else {
        rannot->non_coop = false;
    }

    if(!gm_node.group) {
        rannot->group = false;
    }
    if(!gm_node.divisible) {
        rannot->divisible = false;
    }

    rannot->related_goal = get_node_name(gm_node.text);

    for(general_annot* child : rannot->children) {
        if(child->type == OPERATOR) {
            recursive_fill_up_runtime_annot(child, gm_node);
        }
    }
}

/*
    Function: recursive_child_replacement
    Objective: Replacing the children nodes of a given annotation, since we are dealing with references.
    This is needed in order to deal with forAll statements runtime annotations

    @ Input 1: A reference to the copy runtime annotation to be created
    @ Input 2: A reference to the original runtime annotation
    @ Output: Void. The copy runtime annotation is initialized with the values of the original one
*/ 
void recursive_child_replacement(general_annot* copy, general_annot* original) {
    if(original->children.size() > 0) {
        for(general_annot* original_child : original->children) {
            general_annot* child_copy = new general_annot();

            child_copy->content = original_child->content;
            child_copy->type = original_child->type;
            child_copy->related_goal = original_child->related_goal;
            child_copy->non_coop = original_child->non_coop;
            child_copy->group = original_child->group;
            child_copy->divisible = original_child->divisible;
            recursive_child_replacement(child_copy, original_child);

            child_copy->parent = copy;
            copy->children.push_back(child_copy);
        }
    }
}

/*
    Function: rename_at_instances_in_runtime_annot
    Objective: Rename AT instances in goal model runtime annotation

    @ Input 1: The goal model runtime annotation
    @ Input 2: The at instances map
    @ Output: Void. The goal model runtime annotation has the AT's instances renamed
*/ 
void rename_at_instances_in_runtime_annot(general_annot* gmannot, map<string,vector<AbstractTask>> at_instances) {
    map<string,int> at_instances_counter;

    map<string,vector<AbstractTask>>::iterator at_inst_it;
    for(at_inst_it = at_instances.begin();at_inst_it != at_instances.end();at_inst_it++) {
        string task_id;
        if(at_inst_it->second.at(0).id.find("_") != string::npos) {
            task_id = at_inst_it->second.at(0).id.substr(0,at_inst_it->second.at(0).id.find("_"));
        } else {
            task_id = at_inst_it->second.at(0).id;
        }

        at_instances_counter[task_id] = 1;
    }

    if(gmannot->content == "#" && gmannot->related_goal == "") {
        //Dealing with a forAll in the root
        for(general_annot* child : gmannot->children) {
            recursive_at_instances_renaming(child, at_instances_counter, true);
        }
    } else {
        for(general_annot* child : gmannot->children) {
            recursive_at_instances_renaming(child, at_instances_counter, false);
        }
    }
}

/*
    Function: recursive_at_instances_renaming
    Objective: Rename AT instances in the runtime annotation object and recursive call this renaming step

    @ Input 1: The runtime annotation being considered
    @ Input 2: The counter in order to know which instance we left
    @ Input 3: Boolean flag to know if we have a forAll generated node in the root
    @ Output: Void. The runtime goal model annotation is renamed
*/ 
void recursive_at_instances_renaming(general_annot* rannot, map<string,int>& at_instances_counter, bool in_forAll) {
    set<string> operators {";","#","FALLBACK","OPT","|"};

    set<string>::iterator op_it;

    op_it = operators.find(rannot->content);

    std::cout << "rannot->content: " << rannot->content << std::endl;

    if(op_it == operators.end()) { //If we have a task
        if(rannot->type == TASK) {
            string aux = rannot->content;
            rannot->content = rannot->content + "_" + to_string(at_instances_counter[rannot->content]);

            at_instances_counter[aux]++;
        } else {
            if(rannot->type == MEANSEND) {
                general_annot* child = rannot->children.at(0);
                recursive_at_instances_renaming(child, at_instances_counter, in_forAll);
            }
        }
    } else {
        if(rannot->content == "#" && rannot->related_goal == "") {
            for(general_annot* child : rannot->children) {
                recursive_at_instances_renaming(child, at_instances_counter, true);
            }
        } else {
            for(general_annot* child : rannot->children) {
                recursive_at_instances_renaming(child, at_instances_counter, in_forAll);
            }
        }
    }
}   

/*
    Function: print_runtime_annot_from_general_annot
    Objective: Print the given runtime annotation in the terminal

    @ Input: The runtime annotation to be printed 
    @ Output: Void. The runtime annotation is printed
*/ 
void print_runtime_annot_from_general_annot(general_annot* rt) {
    string rt_annot = "";

    rt_annot = recursive_rt_annot_build(rt);

    cout << rt_annot << endl;
}

/*
    Function: recursive_rt_annot_build
    Objective: Build the runtime annotation string recursively

    @ Input: The current runtime annotation being considered
    @ Output: The current runtime annotation string
*/ 
string recursive_rt_annot_build(general_annot* rt) {
    set<string> operators {";","#","FALLBACK","OPT","|"};

    set<string>::iterator op_it;

    op_it = operators.find(rt->content);

    string annot = "";
    if(rt->non_coop) {
        annot += "NC(";
    }
    if(op_it != operators.end()) {
        if(rt->content == "OPT") {
            string child = recursive_rt_annot_build(rt->children.at(0));

            annot += "OPT(" + child + ")";
        } else if(rt->content == "FALLBACK") {
            vector<string> children;
            for(general_annot* child : rt->children) {
                children.push_back(recursive_rt_annot_build(child));
            }

            annot += "FALLBACK(";
            int cnt = 0;
            for(string c : children) {
                annot += c;
                cnt++;
                if(cnt == 1) {
                    annot += ",";
                } else {
                    annot += ")";
                }
            }
        } else {
            vector<string> children;
            for(general_annot* child : rt->children) {
                children.push_back(recursive_rt_annot_build(child));
            }

            if(!rt->non_coop) {
                annot += "(";
            }
            unsigned int cnt = 0;
            for(string c : children) {
                annot += c;
                if(cnt < rt->children.size()-1) {
                    annot += rt->content;
                } else {
                    annot += ")";
                }
                cnt++;
            }
        }
    } else {
        if(rt->type == MEANSEND) {
            annot += recursive_rt_annot_build(rt->children.at(0));
        } else {
            annot += rt->content;
        }
    }

    if(rt->non_coop) {
        annot += ")";
    }

    return annot;
}

void solve_query_statement(pt::ptree queried_tree, QueriedProperty q, GMGraph gm, int node_id, std::map<std::string,std::pair<std::string,std::vector<pt::ptree>>>& valid_variables) {
    vector<pt::ptree> aux;
				
	if(!queried_tree.empty()) {
		BOOST_FOREACH(pt::ptree::value_type& child, queried_tree) {
			if(child.first == q.query_var.second) {
				if(q.query.size() == 1) {
					if(q.query.at(0) != "") {
						string prop = q.query.at(0).substr(q.query.at(0).find('.')+1);
						bool prop_val;
						istringstream(boost::to_lower_copy(child.second.get<string>(prop))) >> std::boolalpha >> prop_val;
						if(q.query.at(0).find('!') != string::npos) {
							prop_val = !prop_val;
						}
						if(prop_val) aux.push_back(child.second);
					} else {
						aux.push_back(child.second);
					}
				} else {
					string prop = q.query.at(0).substr(q.query.at(0).find('.')+1);
					string prop_val;
					try {
						prop_val = child.second.get<string>(prop);
					} catch(...) {
						string bad_condition = "Cannot solve condition in QueriedProperty of Goal " + get_node_name(gm[node_id].text); 

						throw std::runtime_error(bad_condition);
					}

					bool result;
					if(q.query.at(1) == "==") {
						result = (prop_val == q.query.at(2));
					} else {
						result = (prop_val != q.query.at(2));
					}
					if(result) aux.push_back(child.second);
				}
			}
		}

		string var_name = std::get<vector<pair<string,string>>>(gm[node_id].custom_props["Controls"]).at(0).first;
		string var_type = std::get<vector<pair<string,string>>>(gm[node_id].custom_props["Controls"]).at(0).second;

		valid_variables[var_name] = make_pair(var_type,aux);
	}
}