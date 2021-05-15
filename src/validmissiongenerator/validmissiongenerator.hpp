#ifndef __VALID_MISSION_GENERATOR
#define __VALID_MISSION_GENERATOR

#include <map>
#include <vector>
#include <string>
#include <variant>
#include <queue>
#include <set>

#include "../missiondecomposer/missiondecomposer.hpp"
#include "../utils/constraint.hpp"
#include "../utils/validmissiongeneratorutils.hpp"

class ValidMissionGenerator {
    public:
        ValidMissionGenerator(ATGraph md, GMGraph g, std::vector<Constraint> mc, std::vector<ground_literal> ws, std::vector<std::pair<ground_literal,int>> wsf, std::vector<SemanticMapping> sm, std::map<std::string, std::variant<std::pair<std::string,std::string>,std::pair<std::vector<std::string>,std::string>>> gmvmap);

        std::vector<std::vector<std::pair<int,ATNode>>> generate_valid_mission_decompositions();

        std::map<int,std::vector<std::variant<ground_literal,std::pair<ground_literal,int>>>> recursive_valid_mission_decomposition(std::string last_op, std::queue<std::pair<int,ATNode>>& mission_queue, int depth, std::map<int, std::vector<std::variant<ground_literal,std::pair<ground_literal,int>>>> effects_to_apply);

        void check_conditions(std::map<int, std::vector<std::variant<ground_literal,std::pair<ground_literal,int>>>> effects_to_apply, std::pair<int,ATNode> current_node);
        
        std::vector<ground_literal> apply_pred_effects(std::map<int,std::vector<ground_literal>> pred_eff, set<int> tasks_to_consider);
        std::vector<std::pair<ground_literal,int>> apply_func_effects(std::map<int,std::vector<std::pair<ground_literal,int>>> func_eff, set<int> tasks_to_consider);

        std::queue<std::pair<int,ATNode>> generate_mission_queue();

    private:
        ATGraph mission_decomposition;
        GMGraph gm;
        std::vector<Constraint> mission_constraints;
        std::vector<ground_literal> world_state;
        std::vector<std::pair<ground_literal,int>> world_state_functions;
        std::vector<std::pair<std::vector<std::pair<int,ATNode>>,std::set<int>>> valid_mission_decompositions; 
        std::vector<SemanticMapping> semantic_mapping;
        std::map<std::string, std::variant<std::pair<std::string,std::string>,std::pair<std::vector<std::string>,std::string>>> gm_var_map;
};

#endif