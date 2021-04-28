#ifndef __FILEOUTPUTGENERATORFACTORY
#define __FILEOUTPUTGENERATORFACTORY

#include "xmloutputgenerator.hpp"
#include "outputgenerator.hpp"

class FileOutputGeneratorFactory {
    public:
        std::shared_ptr<FileOutputGenerator> create_file_output_generator(GMGraph gm, ATGraph mission_decomposition, std::vector<ground_literal> world_state, std::pair<std::string,std::string> output);
};

#endif